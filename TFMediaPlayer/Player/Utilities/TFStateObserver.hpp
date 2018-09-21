//
//  TFStateObserver.hpp
//  TFMediaPlayer
//
//  Created by shiwei on 2018/9/20.
//  Copyright © 2018年 shiwei. All rights reserved.
//

//An observer that catch all states in player to help debug.

#ifndef TFStateObserver_hpp
#define TFStateObserver_hpp

#include <stdio.h>
#include <string>
#include <map>
#include <mach/mach_time.h>

#define myStateObserver (*TFStateObserver::shareInstance())

using namespace std;
class TFStateObserver {
    
    double currentTime(){
        uint64_t cur = mach_absolute_time();
        
        mach_timebase_info_data_t timebase;
        mach_timebase_info(&timebase);
        
        return cur*1e-9*(double)timebase.numer/timebase.denom;
    }
    
    map<string, int> counts;
    map<string, double> timeMarks;
    map<string, string> labels;
    
public:
    
    static TFStateObserver* shareInstance(){
        
        static TFStateObserver *instance;
        
        if (instance == nullptr) {
            instance = new TFStateObserver();
        }
        
        return instance;
    };
    
    map<string, int>& getCounts(){
        return counts;
    }
    
    map<string, double>& getTimeMarks(){
        return timeMarks;
    }
    
    map<string, string>& getLabels(){
        return labels;
    }
    
    void mark(string name, int count, bool additive = false){
        if (additive) {
            counts[name] += count;
        }else{
            counts[name] = count;
        }
    }
    
    void timeMark(string name){
        timeMarks[name] = currentTime();
    }
    
    void labelMark(string name, string label){
        labels[name] = label;
    }
};



#endif /* TFStateObserver_hpp */
