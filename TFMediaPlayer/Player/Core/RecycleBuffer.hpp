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
        
        size_t limitSize = 0;
        size_t allocedSize = 0;
        size_t usedSize = 0;
        
        RecycleNode *frontNode = nullptr;  //the newest used node
        RecycleNode *backNode = nullptr;  // the oldest used node
        
        bool expand(){
            if (allocedSize >= limitSize) {
                return false;
            }
            
            size_t expandSize = allocedSize < (limitSize-allocedSize) ? allocedSize : (limitSize-allocedSize);
            
            RecycleNode *nextNode = frontNode;
            RecycleNode *closeNode = frontNode->pre;
            
            //construct from next to pre.  new node(pre) <----- front(next).
            for (size_t i = 0; i<expandSize; i++) {
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
        
    public:
        
        RecycleBuffer(size_t limitSize = 0, bool allocToLimit = false){
            if (limitSize > 0) {
                this->limitSize = limitSize;
            }
            
            if (limitSize && allocToLimit) {
                
                allocedSize = limitSize;
            }else{
                allocedSize = 8; //init size
            }
            
            
            frontNode = new RecycleNode();
            RecycleNode *lastNode = frontNode;
            
            for (size_t i = 1; i<allocedSize; i++) {
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
        
        bool isFull(){
            return usedSize == limitSize;
        };
        bool isEmpty(){
            return usedSize == 0;
        }
        
        bool insert(T val){
            if (usedSize == allocedSize) {
                if (!expand()) {
                    //stop pushing until there is unused node.
                    return false;
                }
            }
            
            frontNode->pre->val = val;
            frontNode = frontNode->pre;
            
            usedSize++;
            
            return true;
        }
        
        bool getOut(T *valP){
            
            if (usedSize == 0) {
                return false;
            }
            
            if (valP) *valP = backNode->val;
            
            backNode = backNode->pre;
            
            usedSize--;
            
            return true;
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
    };
}

#endif /* RecycleBuffer_hpp */
