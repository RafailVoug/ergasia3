int main() {
    srand(time(NULL)); // Initialize random number generator

    int shmid = shmget(IPC_PRIVATE, sizeof(Product) * NUM_PRODUCTS, IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("Shared memory creation failed");
        exit(EXIT_FAILURE);
    }

    Product *catalog = (Product *)shmat(shmid, NULL, 0);
    if (catalog == (void *)-1) {
        perror("Shared memory attachment failed");
        exit(EXIT_FAILURE);
    }

    initialize_catalog(catalog);

    sem_t *sem = sem_open("catalog_sem", O_CREAT, 0644, 1);
    if (sem == SEM_FAILED) {
        perror("Semaphore creation failed");
        exit(EXIT_FAILURE);
    }

    for (int customer = 1; customer <= NUM_CUSTOMERS; customer++) {
        pid_t pid = fork();

        if (pid == 0) {
            // Child process (customer)
            int sockfd;
            struct sockaddr_in serv_addr;

            if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                perror("Socket creation failed");
                exit(EXIT_FAILURE);
            }

            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(PORT_BASE + customer);
            serv_addr.sin_addr.s_addr = INADDR_ANY;

            while (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                sleep(1); // Retry until the parent binds
            }

            double total_spent = 0.0;
            int orders[ORDERS_PER_CUSTOMER];
            generate_orders(orders, ORDERS_PER_CUSTOMER, NUM_PRODUCTS);

            for (int order = 0; order < ORDERS_PER_CUSTOMER; order++) {
                int product_id = orders[order];
                write(sockfd, &product_id, sizeof(product_id));

                int success;
                double price;
                read(sockfd, &success, sizeof(success));
                read(sockfd, &price, sizeof(price));

                total_spent += success ? price : 0.0;

                printf("Customer %d: Product %d -> %s, Price: %.2f\n", 
                       customer, product_id + 1, success ? "Success" : "Failure", success ? price : 0.0);

                sleep(1);
            }

            printf("Customer %d: Total Spent: %.2f\n", customer, total_spent);

            close(sockfd);
            exit(0);
        }
    }

    // Parent process (server)
    int sockets[NUM_CUSTOMERS];
    struct sockaddr_in serv_addrs[NUM_CUSTOMERS], cli_addrs[NUM_CUSTOMERS];
    socklen_t cli_len = sizeof(struct sockaddr_in);

    for (int customer = 1; customer <= NUM_CUSTOMERS; customer++) {
        if ((sockets[customer - 1] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }

        serv_addrs[customer - 1].sin_family = AF_INET;
        serv_addrs[customer - 1].sin_port = htons(PORT_BASE + customer);
        serv_addrs[customer - 1].sin_addr.s_addr = INADDR_ANY;

        if (bind(sockets[customer - 1], (struct sockaddr *)&serv_addrs[customer - 1], sizeof(serv_addrs[customer - 1])) < 0) {
            perror("Bind failed");
            exit(EXIT_FAILURE);
        }

        if (listen(sockets[customer - 1], 1) < 0) {
            perror("Listen failed");
            exit(EXIT_FAILURE);
        }
    }

    for (int customer = 1; customer <= NUM_CUSTOMERS; customer++) {
        int connfd = accept(sockets[customer - 1], (struct sockaddr *)&cli_addrs[customer - 1], &cli_len);
        if (connfd < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        for (int order = 0; order < ORDERS_PER_CUSTOMER; order++) {
            int product_id;
            read(connfd, &product_id, sizeof(product_id));

            int success = 0;
            double revenue = 0.0;
            process_order(catalog, product_id, &success, &revenue, sem);

            write(connfd, &success, sizeof(success));
            write(connfd, &revenue, sizeof(revenue));
        }

        close(connfd);
        close(sockets[customer - 1]);
    }

    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        wait(NULL);
    }

    generate_report(catalog);

    sem_close(sem);
    sem_unlink("catalog_sem");
    shmdt(catalog);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}
