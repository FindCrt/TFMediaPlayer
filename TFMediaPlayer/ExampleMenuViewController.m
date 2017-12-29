//
//  ExampleMenuViewController.m
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/25.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#import "ExampleMenuViewController.h"
#import "TFNetMp4PlayViewController.h"
#import "TFLocalMp4ViewController.h"

#import "UnitTest.h"

@interface ExampleMenuViewController (){
    NSArray *_exampleInfos;
}

@end

@implementation ExampleMenuViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    _exampleInfos = @[
                      @{@"title": @"本地mp4视频", @"actionVC": @"TFLocalMp4ViewController"},
                      @{@"title": @"播放网络mp4视频", @"actionVC": @"TFNetMp4PlayViewController"}
                      ];
    
    //test
    
//    dispatch_async(dispatch_get_global_queue(0, 0), ^{
//        UnitTest *test = [[UnitTest alloc] init];
//        [test testRecycleBuffer];
//    });
}

#pragma mark - Table view data source

-(NSInteger)numberOfSectionsInTableView:(UITableView *)tableView{
    return 1;
}

-(NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section{
    return _exampleInfos.count;
}

-(UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath{
    NSDictionary *info = _exampleInfos[indexPath.row];
    
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"cell"];
    if (!cell) {
        cell = [[UITableViewCell alloc] initWithStyle:(UITableViewCellStyleDefault) reuseIdentifier:@"cell"];
    }
    
    cell.textLabel.text = [info objectForKey:@"title"];
    cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    
    return cell;
}

-(void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath{
    [tableView deselectRowAtIndexPath:indexPath animated:NO];
    
    NSDictionary *info = _exampleInfos[indexPath.row];
    UIViewController *actionVC = [[NSClassFromString([info objectForKey:@"actionVC"]) alloc]init];
    
    NSDictionary *params = [info objectForKey:@"params"];
    if (params) {
        [actionVC setValuesForKeysWithDictionary:params];
    }
    
    [self.navigationController pushViewController:actionVC animated:YES];
}

@end
