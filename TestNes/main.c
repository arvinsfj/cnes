//
//  main.c
//  TestNes
//
//  Created by arvin on 2017/8/21.
//  Copyright © 2017年 com.fuwo. All rights reserved.
//

#include "cnes.h"

int main(int argc, const char * argv[])
{
    if (argc < 2) {
        printf("Please enter a rom name!!!\n");
        return 0;
    }
    
    cnes_init(argv[1]);
    
    return 0;
}
