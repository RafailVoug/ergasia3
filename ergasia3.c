int main() {
    srand(time(NULL)); // Initialize random number generator

    // Create shared memory for catalog
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

    // Initialize catalog and semaphore
    initialize_catalog(catalog);
    sem_t *sem = sem_open("catalog_sem", O_CREAT, 0644, 1);
    if (sem == SEM_FAILED) {
        perror("Semaphore creation failed");
        exit(EXIT_FAILURE);
    }

    // Create customer processes
    for (int customer = 1; customer <= NUM_CUSTOMERS; customer++) {
        pid_t pid = fork();

        if (pid == 0) {
            // Child process (customer)
            double total_spent = 0.0;
            int orders[ORDERS_PER_CUSTOMER];
            generate_orders(orders, ORDERS_PER_CUSTOMER, NUM_PRODUCTS);

            for (int order = 0; order < ORDERS_PER_CUSTOMER; order++) {
                int product_id = orders[order];
                int success;
                double revenue;

                process_order(catalog, product_id, &success, &revenue, sem);

                printf("Customer %d: Product %d -> %s, Price: %.2f\n", 
                       customer, product_id + 1, success ? "Success" : "Failure", success ? revenue : 0.0);

                total_spent += success ? revenue : 0.0;
                sleep(1);
            }

            printf("Customer %d: Total Spent: %.2f\n", customer, total_spent);
            exit(0);
        }
    }

    // Wait for all customers to finish
    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        wait(NULL);
    }

    // Generate final report
    generate_report(catalog);

    // Clean up resources
    sem_close(sem);
    sem_unlink("catalog_sem");
    shmdt(catalog);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}
