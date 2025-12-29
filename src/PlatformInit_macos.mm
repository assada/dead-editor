#import <Foundation/Foundation.h>

void platform_init() {
    [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"AppleMomentumScrollSupported"];
}

