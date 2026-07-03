void checkAndCount(void) {
    int count;
    int limit;

    count = 0;
    limit = 5;

    do {
        count = count + 1;
    } while (count <= limit); 
    if (count == 6) {
        int success;
        success = 1;
    } else {
        int failure;
        failure = 0;
    }
    
    return;
}