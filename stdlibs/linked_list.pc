/*
    Linked List
*/

#include <stddef.h>

// Definition eines Elements in der Liste
typedef struct Node {
    void* data;
    struct Node* prev;
    struct Node* next;
} Node;

// Definition der doppelt verketteten Liste
typedef struct LinkedList {
    Node* head;
    Node* tail;
} LinkedList;

// Funktion zum Einfügen eines Elements am Anfang der Liste
void insertAtBeginning(LinkedList* list, void* data) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->data = data;
    newNode->prev = NULL;
    newNode->next = list->head;

    if (list->head != NULL)
        list->head->prev = newNode;
    else
        list->tail = newNode;

    list->head = newNode;
}

// Funktion zum Einfügen eines Elements am Ende der Liste
void insertAtEnd(LinkedList* list, void* data) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->data = data;
    newNode->prev = list->tail;
    newNode->next = NULL;

    if (list->tail != NULL)
        list->tail->next = newNode;
    else
        list->head = newNode;

    list->tail = newNode;
}

// Funktion zum Entfernen eines Elements aus der Liste
void removeFromList(LinkedList* list, Node* node) {
    if (node->prev != NULL)
        node->prev->next = node->next;
    else
        list->head = node->next;

    if (node->next != NULL)
        node->next->prev = node->prev;
    else
        list->tail = node->prev;

    free(node);
}

// Funktion zum Ausgeben der Liste
void printList(const LinkedList* list) {
    Node* current = list->head;
    while (current != NULL) {
        printf("Node Address: %p", current);
        printf(" Data: %d\n", *(int*)current->data);
        current = current->next;
    }
}

// Funktion zum Freigeben des Speichers der Liste
void freeList(LinkedList* list) {
    Node* current = list->head;
    while (current != NULL) {
        Node* temp = current;
        current = current->next;
        free(temp);
    }
}

int ll_main() {
    LinkedList list = { NULL, NULL };

    int value1 = 1;
    int value2 = 2;
    int value3 = 3;

    insertAtEnd(&list, &value1);
    insertAtEnd(&list, &value2);
    insertAtBeginning(&list, &value3);
    
    printf("List:\n");
    printList(&list);

    Node* nodeToRemove = list.head->next;
    removeFromList(&list, nodeToRemove);

    printf("List after removal:\n");
    printList(&list);

    freeList(&list);

    return 0;
}
