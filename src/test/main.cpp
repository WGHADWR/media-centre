//
// Created by smzhkj on 23-2-28.
//

#include <iostream>
#include "../util/encrypt.h"

using namespace std;

int main() {
    MD5Digest md5Encode("123456");
    cout << md5Encode << endl;

    cout << md5("123456") << endl;

    return 0;
}