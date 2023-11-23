#include <stdio.h>

int insertion_sort(int *arr, size_t size)
{
    int j;
    for (j = 1; j < size; ++j) {
        int key = arr[j];
        int i;
        for (i = j-1; (i >= 0) && (arr[i]>key); --i) {
            arr[i+1] = arr[i];
        }
        arr[i+1] = key;
    }

    return 0;
}

void print(int *arr, size_t size)
{
    int i;
    for (i = 0; i < size; ++i)
    {
        printf("%d, ", arr[i]);
    }
    printf("\n");
}

int main()
{
    int arr[10] = {1,3,5,7,9,2,4,6,8,0};
    print(arr, 10);

    insertion_sort(arr, 10);
    print(arr, 10);

    return 0;
}
