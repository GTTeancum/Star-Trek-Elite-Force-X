#include "../../code/win32/snd_voice_spatial.h"
#include <assert.h>
#include <stdio.h>
static bool near(float a,float b) { return fabs(a-b)<0.0001f; }
int main() {
    StefxVoiceGains center=StefxSpatialVoiceGains(0,0,0,300,1);
    assert(near(center.left,center.right));
    assert(near(center.left*center.left+center.right*center.right,1));
    StefxVoiceGains right=StefxSpatialVoiceGains(100,0,0,300,1);
    StefxVoiceGains left=StefxSpatialVoiceGains(-100,0,0,300,1);
    assert(right.left==0 && left.right==0);
    assert(near(right.right,left.left));
    StefxVoiceGains up=StefxSpatialVoiceGains(0,100,0,300,1);
    assert(near(up.left,up.right));
    assert(near(up.left*up.left+up.right*up.right,right.right*right.right));
    for (int n=1;n<=4;n++) for(int d=0;d<=700;d+=10) {
        for(int angle=0;angle<360;angle+=5) {
            float r=angle*3.14159265f/180.0f;
            StefxVoiceGains g=StefxSpatialVoiceGains(d*(float)sin(r),0,d*(float)cos(r),300,n);
            assert(g.left>=0 && g.left<=1 && g.right>=0 && g.right<=1);
            StefxVoiceGains mirror=StefxSpatialVoiceGains(-d*(float)sin(r),0,d*(float)cos(r),300,n);
            assert(near(g.left,mirror.right) && near(g.right,mirror.left));
            if(d>=600) assert(g.left==0 && g.right==0);
            float gain=(float)sqrt(g.left*g.left+g.right*g.right);
            assert(gain*n<=1.0001f);
            StefxVoiceGains farther=StefxSpatialVoiceGains((d+1)*(float)sin(r),0,(d+1)*(float)cos(r),300,n);
            assert(farther.left*farther.left+farther.right*farther.right<=gain*gain+0.0001f);
        }
    }
    StefxVoiceGains half=StefxSpatialVoiceGains(0,0,300.5f,300,1);
    assert(near(half.left*half.left+half.right*half.right,0.25f));
    puts("PASS: 20,448 spatial cases; rotation, side, height, distance, mute boundary and listener headroom");
    return 0;
}
