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
#include <vector>
extern "C"{
#include <libavutil/time.h>
}

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
        
        /** Use this to observe the change of usedsize. It makes you know RecycleBuffer's status to do specific things. 
         * if return true, remove the observer.
         */
        typedef bool (*ObserverNotifyFunc)(RecycleBuffer *buffer, int curSize, bool isGreater, void *context);
        class UsedSizeObserver{
            void *observer = nullptr;
            int checkSize = defaultInitAllocSize;
            bool isGreater = true;
            ObserverNotifyFunc notifyFunc = nullptr;
            
            friend RecycleBuffer;
            
            UsedSizeObserver(void *observer, int checkSize, bool isGreater, ObserverNotifyFunc notifyFunc):observer(observer),checkSize(checkSize),isGreater(isGreater),notifyFunc(notifyFunc){};
        };
        
        const static int defaultInitAllocSize = 8;
        
        long limitSize = LONG_MAX;
        long allocedSize = 0;
        long usedSize = 0;
        
        RecycleNode *frontNode = nullptr;  //the newest used node
        RecycleNode *backNode = nullptr;  // the oldest used node
        
        //The node which has been blocked in inserting tube, we must clear it before flush buffer.
        T *insertingVal = nullptr;
        
        pthread_cond_t inCond = PTHREAD_COND_INITIALIZER;
        pthread_cond_t outCond = PTHREAD_COND_INITIALIZER;
        pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
        
        bool ioDisable = false;
        
        std::vector<UsedSizeObserver *> observers;
        
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
            
            RecycleBufferLog("insert: %s[%ld],[%x->%x,%x->%x]\n",name,usedSize,frontNode,frontNode->val, backNode,backNode->val);
            
            if (usedSize > allocedSize/2) {
                pthread_cond_signal(&outCond);
            }
            
            if (!observers.empty()) {
                for (auto iter = observers.begin(); iter != observers.end();) {
                    UsedSizeObserver *ob = *iter;
                    if (ob->isGreater && ob->checkSize < usedSize) {
                        bool shouldRemove = ob->notifyFunc(this, ob->checkSize, ob->isGreater, ob->observer);
                        if (shouldRemove) {
                            iter = observers.erase(iter);
                        }else{
                            iter++;
                        }
                    }else{
                        iter++;
                    }
                }
            }
            
            
            return true;
        }
        
        bool getOut(T *valP){
            
            if (usedSize == 0) {
                return false;
            }
            
            if (valP) *valP = backNode->val;
            
            backNode = backNode->pre;
            
            usedSize--;
            
            RecycleBufferLog("getout: %s[%ld],[%x->%x,%x->%x]\n",name,usedSize,frontNode,frontNode->val, backNode,backNode->val);
            
            if (usedSize < allocedSize/2) {
                pthread_cond_signal(&inCond);
            }
            
            if (!observers.empty()) {
                for (auto iter = observers.begin(); iter != observers.end();) {
                    UsedSizeObserver *ob = *iter;
                    if (!ob->isGreater && ob->checkSize > usedSize) {
                        bool shouldRemove = ob->notifyFunc(this, ob->checkSize, ob->isGreater, ob->observer);
                        if (shouldRemove) {
                            iter = observers.erase(iter);
                        }else{
                            iter++;
                        }
                    }else{
                        iter++;
                    }
                }
            }
            
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
                insertingVal = &val;
                pthread_mutex_lock(&mutex);
                pthread_cond_wait(&inCond, &mutex);
                pthread_mutex_unlock(&mutex);
                RecycleBufferLog("<<<<unlock full %s\n",name);
            }
            
            if (ioDisable) {
                if (valueFreeFunc) valueFreeFunc(&val);
            }else{
                insert(val);
//                pthread_cond_signal(&cond);
            }
            insertingVal = nullptr;
        }
        
        void blockGetOut(T *valP){

            if (!ioDisable && usedSize == 0) {
                RecycleBufferLog(">>>>lock empty %s\n",name);
                pthread_mutex_lock(&mutex);
                pthread_cond_wait(&outCond, &mutex);
                pthread_mutex_unlock(&mutex);
                RecycleBufferLog("<<<<unlock empty %s[%d,%d]\n",name,usedSize,allocedSize);
            }
            
            if (!ioDisable) {
                getOut(valP);
//                pthread_cond_signal(&cond);
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
        
        void addObserver(void *observer, int checkSize, bool isGreater, ObserverNotifyFunc notifyFunc){
            if (notifyFunc == nullptr) {
                return;
            }
            observers.push_back(new UsedSizeObserver(observer, checkSize, isGreater, notifyFunc));
        }
        
        void removeObserver(void *observer, int checkSize, bool isGreater){
            
            for (auto iter = observers.begin(); iter != observers.end(); iter++) {
                auto ob = *iter;
                if (ob->observer == observer &&
                    ob->checkSize == checkSize &&
                    ob->isGreater == isGreater) {
                    observers.erase(iter);
                    break;
                }
            }
        }
        
        /** remove all inserted data */
        void flush(){
            RecycleBufferLog("signalAllBlock 1\n");
            ioDisable = true;
            pthread_cond_broadcast(&inCond);
            pthread_cond_broadcast(&outCond);
            RecycleBufferLog("signalAllBlock 2\n");
            
            while (insertingVal != nullptr) {
                //sleep
            }
            
            //free valid datas
            if (usedSize > 0 && valueFreeFunc != nullptr) {
                RecycleNode *curNode = frontNode;
                do {
                    usedSize--;
                    //av_usleep(10000);
                    RecycleBufferLog("free: %s[%ld],[%x->%x,%x->%x],",name,usedSize,frontNode,frontNode->val, backNode,backNode->val);
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
            
            observers.clear();
            
            ioDisable = true;
            
            RecycleBufferLog("ioDisable end\n");
        }
    };
}

#endif /* RecycleBuffer_hpp */
