#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include <machine/types.h>

template <class T>
struct Node
{
    T val;
    Node<T> *next;
    Node<T> *prev;
};

template <class T>
class LinkedList
{
public:
    class Iterator;

    LinkedList() { first = last = NULL; }
    ~LinkedList() { clear(); }

    void insertAtBack(T valueToInsert)
    {
        Node<T> *tmp = new Node<T>();
        tmp->val = valueToInsert;

        if (last == NULL) {
            first = last = tmp;
        } else {
            last->next = tmp;
            tmp->prev = last;
            last = tmp;
        }
    }

    void insertAtFront(T valueToInsert)
    {
        Node<T> * newNode = new Node<T>();
        newNode->val = valueToInsert;

        if(first == NULL)
        {
            first = last = newNode;
        }
        else
        {
            newNode->next = first;
            first->prev = newNode;
            first = newNode;
        }
    }

    Iterator insertAfter(Iterator& node, T valueToInsert)
    {
        if (node.currentNode == last) {
            insertAtBack(valueToInsert);
            return node;
        }

        if (node.currentNode == NULL) {
            insertAtBack(valueToInsert);
            return end();
        }

        Node<T> * newNode = new Node<T>();
        newNode->val = valueToInsert;
        newNode->next = node.currentNode->next;
        newNode->prev = node.currentNode;
        node.currentNode->next = newNode;

        return node;
    }

    Iterator insertBefore(Iterator& node, T valueToInsert)
    {
        if (node.currentNode == first) {
            insertAtFront(valueToInsert);
            return node;
        }

        if (node.currentNode == NULL) {
            insertAtFront(valueToInsert);
            return begin();
        }

        Node<T> * newNode = new Node<T>();
        newNode->val = valueToInsert;
        newNode->next = node.currentNode;
        newNode->prev = node.currentNode->prev;
        node.currentNode->prev = newNode;

        return node;
    }

    bool removeFromFront()
    {
        if (isEmpty())
            return false;

        if (first == last) {
            delete(first);
            first = last = NULL;
            return true;
        }

        Node<T> *tmp = first;
        first = tmp->next;
        first->prev = NULL;

        delete(tmp);
        return true;
    }

    bool removeFromBack()
    {
        if (isEmpty())
            return false;

        if (first == last) {
            delete(last);
            first = last = NULL;
            return true;
        }

        Node<T> *tmp = last;
        last = tmp->prev;
        last->next = NULL;

        delete(tmp);
        return true;
    }

    Iterator removeAt(Iterator& node) {
        if (node.currentNode == NULL)
            return node;

        Node<T> * tmp = node.currentNode;

        if (tmp->prev) {
            tmp->prev->next = tmp->next;
        }
        if (tmp->next) {
            tmp->next->prev = tmp->prev;
        }

        if (tmp == first)
            first = tmp->next;

        if (tmp == last)
            last = tmp->prev;

        Iterator retval(tmp->next ? tmp->next : tmp->prev);
        delete(tmp);
        return retval;
    }

    bool isEmpty()
    {
        return first == NULL && last == NULL;
    }

    int size()
    {
        int count = 0;
        for (Iterator it = begin(); it.valid(); ++it()) {
            ++count;
        }
        return count;
    }

    void clear()
    {
        while(removeFromFront())
        {
            // noop
        }
    }

    Iterator begin() const { return Iterator(first); }
    Iterator end() const { return Iterator(last); }

    class Iterator
    {
        friend LinkedList;

    public:
        T& operator*() const { return currentNode->val; }
        T& operator*(T value) { return currentNode->val = value; }
        T& operator->() const { return currentNode->val; }

        // prefix
        Iterator& operator++()
        {
            if (currentNode)
                currentNode = currentNode->next;
            return *this;
        }
        // postfix
        Iterator operator++(int)
        {
            Iterator tmp(*this);
            operator++();
            return tmp;
        }

        // prefix
        Iterator& operator--()
        {
            if (currentNode)
                currentNode = currentNode->prev;
            return *this;
        }
        // postfix
        Iterator operator--(int)
        {
            Iterator tmp(*this);
            operator--();
            return tmp;
        }

        bool operator==(const Iterator& rhs) const
        {
            return currentNode == rhs.currentNode;
        }

        bool operator!=(const Iterator& rhs) const
        {
            return !(operator==(rhs));
        }

        bool valid() const {
            return currentNode;
        }
        
    private:
        Iterator(Node<T> * node)
        {
            currentNode = node;
        }
        
        Node<T> *currentNode;
    };
    
private:
    Node<T> *first;
    Node<T> *last;
};

#endif
