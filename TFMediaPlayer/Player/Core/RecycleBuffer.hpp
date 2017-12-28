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
#include <queue>

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
        
        RecycleNode *list;
        
        size_t limitSize;
        size_t allocedSize;
        size_t usedSize;
        
        RecycleNode *frontNode;  //the newest used node
        RecycleNode *backNode;  //the oldest used node
        
        bool expand(){
            if (allocedSize >= limitSize) {
                return false;
            }
            
            size_t expandSize = allocedSize < (limitSize-allocedSize) ? allocedSize : (limitSize-allocedSize);
            
            RecycleNode *nextNode = frontNode;
            RecycleNode *closeNode = frontNode->pre;
            
            for (size_t i = 0; i<expandSize; i++) {
                RecycleNode *node = new RecycleNode();
                nextNode->pre = node;
                node->next = nextNode;
                
                if (i == expandSize-1) { //link last node with the other node of break.
                    node->pre = closeNode;
                    closeNode->next = node;
                }
            }
            
            allocedSize += expandSize;
            
            std::queue<int> qq;
            
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
            
            list = (RecycleNode *)malloc(sizeof(RecycleNode) * allocedSize);
            
            frontNode = backNode = list;
        }
        
        bool canIn = true;
        
        //TODO: thread safety problem
        bool in(T val){
            if (usedSize == allocedSize) {
                if (!expand()) {
                    //stop pushing until there is unused node.
                    canIn = false;
                    return false;
                }
            }
            
            frontNode->pre->val = val;
            frontNode = frontNode->pre;
            
            usedSize++;
            
            return true;
        }
        
        bool out(T *valP){
            
            if (usedSize == 0) {
                return false;
            }
            
            *valP = backNode->val;
            backNode = backNode->pre;
            
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
