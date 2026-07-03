
int sumArray(int arr[], int size) {
    int sum;
    sum = 0;


    for (int i = 0; i < size; ++i) {
        sum = sum + arr[i]; 
    }

    return sum; 
}


int main(void) {
    int numbers[5]; 
    int total;

    numbers[0] = 10;
    numbers[1] = 20;
    numbers[2] = 30;
    numbers[3] = 40;
    numbers[4] = 50;


    total = sumArray(numbers, 5);

    return total;
}