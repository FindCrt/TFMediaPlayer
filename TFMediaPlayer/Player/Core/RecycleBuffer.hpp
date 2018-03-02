//
//  RecycleBuffer.hpp
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/28.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#ifndef RecycleBuffer_hpp
#define RecycleBuffer_hpp

#include <stdio.h>
#include <pthread.h>
#include <limits.h>

#define RecycleBufferLog(fmt,...) //printf(fmt,##__VA_ARGS__)

/* |->--front-----back------>| */

namespace tfmpcore {
    template<typename T>
    class RecycleBuffer{
        
        class RecycleNode{
            T val;
            struct RecycleNode *pre;
            struct RecycleNode *next;
            
            friend RecycleBuffer;
        };
        
        const static int defaultInitAllocSize = 8;
        
        long limitSize = LONG_MAX;
        long allocedSize = 0;
        long usedSize = 0;
        
        RecycleNode *frontNode = nullptr;  //the newest used node
        RecycleNode *backNode = nullptr;  // the oldest used node
        
        pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
        pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
        
        bool ioDisable = false;
        
        bool expand(){
            if (allocedSize >= limitSize) {
                return false;
            }
            if (allocedSize == 0) {
                allocedSize = defaultInitAllocSize;
                initAlloc();
                return true;
            }
            
            long expandSize = allocedSize < (limitSize-allocedSize) ? allocedSize : (limitSize-allocedSize);
            
            RecycleNode *nextNode = frontNode;
            RecycleNode *closeNode = frontNode->pre;
            
            //construct from next to pre.  new node(pre) <----- front(next).
            for (long i = 0; i<expandSize; i++) {
                RecycleNode *node = new RecycleNode();
                nextNode->pre = node;
                node->next = nextNode;
                
                if (i == expandSize-1) { //link last node with the other node of break.
                    node->pre = closeNode;
                    closeNode->next = node;
                }
                
                nextNode = node;
            }
            
            allocedSize += expandSize;
            
            return true;
        }
        
        void initAlloc(){
            frontNode = new RecycleNode();
            RecycleNode *lastNode = frontNode;
            
            for (long i = 1; i<allocedSize; i++) {
                RecycleNode *node = new RecycleNode();
                
                lastNode->next = node;
                node->pre = lastNode;
                
                if (i == allocedSize-1) { //link last node with the other node of break.
                    node->next = frontNode;
                    frontNode->pre = node;
                }
                
                lastNode = node;
            }
            
            backNode = frontNode->pre;
        }
        
    public:
        
        RecycleBuffer(long limitSize = 0, bool allocToLimit = false){
            if (limitSize > 0) {
                this->limitSize = limitSize;
            }
            
            if (limitSize && allocToLimit) {
                
                allocedSize = limitSize;
            }else{
                allocedSize = defaultInitAllocSize; //init size
            }
            
            initAlloc();
        }
        
        /** name for identifying this RecycleNode */
        char name[16];
        
        /** Use this func to free RecycleNode.val as RecycleBuffer doesn't know what T exactly is. */
        void (*valueFreeFunc)(T *val);
        
        bool isFull(){
            return usedSize == limitSize;
        };
        bool isEmpty(){
            return usedSize == 0;
        }
        
        bool insert(T val){
            if (usedSize >= allocedSize) {
                if (!expand()) {
                    //stop pushing until there is unused node.
                    return false;
                }
            }
            
            frontNode->pre->val = val;
            frontNode = frontNode->pre;
            
            usedSize++;
            RecycleBufferLog("insert: %s[%ld]",name,usedSize);
            
            return true;
        }
        
        bool getOut(T *valP){
            
            if (usedSize == 0) {
                return false;
            }
            
            if (valP) *valP = backNode->val;
            
            backNode = backNode->pre;
            
            usedSize--;
            RecycleBufferLog("getout: %s[%ld]",name,usedSize);
            return true;
        }
        
        /** When it's ioDisable, unblocking thread and freeing node instead inserting it. 
          * Because RecycleBuffer doesn't need or has any node,
          * but blocking may lead to deadlock.
          * For every inserted node, RecycleBuffer take over it's memory management.
         */
        void blockInsert(T val){
            
            if (!ioDisable && usedSize >= limitSize) {
                RecycleBufferLog(">>>>lock full %s\n",name);
                pthread_mutex_lock(&mutex);
                pthread_cond_wait(&cond, &mutex);
                pthread_mutex_unlock(&mutex);
                RecycleBufferLog("<<<<unlock full %s\n",name);
            }
            
            if (ioDisable) {
                if (valueFreeFunc) valueFreeFunc(&val);
            }else{
                insert(val);
                pthread_cond_signal(&cond);
            }
        }
        
        void blockGetOut(T *valP){

            if (!ioDisable && usedSize == 0) {
                RecycleBufferLog(">>>>lock empty %s\n",name);
                pthread_mutex_lock(&mutex);
                pthread_cond_wait(&cond, &mutex);
                pthread_mutex_unlock(&mutex);
                RecycleBufferLog("<<<<unlock empty %s\n",name);
            }
            
            if (!ioDisable) {
                getOut(valP);
                pthread_cond_signal(&cond);
            }
        }
        
        bool back(T *valP){
            if (usedSize == 0) {
                return false;
            }
            *valP = backNode->val;
            
            return true;
        }
        
        bool front(T *valP){
            if (usedSize == 0) {
                return false;
            }
            *valP = frontNode->val;
            
            return true;
        }
        
        /** remove all inserted data */
        void flush(){
            RecycleBufferLog("signalAllBlock 1\n");
            ioDisable = true;
            pthread_cond_broadcast(&cond);
            RecycleBufferLog("signalAllBlock 2\n");
            
            //free valid datas
            if (usedSize > 0 && valueFreeFunc != nullptr) {
                RecycleNode *curNode = frontNode;
                do {
                    valueFreeFunc(&(curNode->val));
                    curNode = curNode->next;
                } while (curNode != backNode->next);
            }
            
            usedSize = 0;
            backNode = frontNode->pre;
            
            ioDisable = false;
        }
        
        /** remove all inserted data and free all alloced nodes. */
        void flushAndFree(){
            
            flush();
            ioDisable = false;
            
            //free all nodes
            RecycleNode *curNode = frontNode;
            do {
                RecycleNode *next = curNode->next;
                free(curNode);
                curNode = next;
            } while (curNode != backNode->next);
            
            frontNode = nullptr;
            backNode = nullptr;
            
            allocedSize = 0;
            
            ioDisable = true;
            
            RecycleBufferLog("ioDisable end\n");
        }
    };
}

#endif /* RecycleBuffer_hpp */
