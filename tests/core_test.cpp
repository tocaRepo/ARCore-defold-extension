#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include "ar_types.h"
#include "ar_image.h"
using namespace arcore;

int main() {
    // Odd dimensions, independent chroma row strides, interleaved chroma pixel
    // strides, and a non-unit luma pixel stride. Padding must never be sampled.
    uint8_t y[24], u[12], v[16];
    for (int i=0;i<24;++i) y[i]=99;
    for (int i=0;i<12;++i) u[i]=255;
    for (int i=0;i<16;++i) v[i]=255;
    for (int row=0;row<3;++row) for (int col=0;col<3;++col) y[row*8+col*2] = row == 0 ? 16 : row == 1 ? 235 : 81;
    for (int row=0;row<2;++row) for (int col=0;col<2;++col) { u[row*6+col*2]=128; v[row*8+col*3]=128; }
    ImagePlane planes[3] = {{y,24,8,2},{u,12,6,2},{v,16,8,3}};
    std::vector<uint8_t> rgb;
    assert(ConvertYuv(3,3,planes,rgb) && rgb.size()==27);
    for(int i=0;i<9;++i) assert(rgb[i]==0);
    for(int i=9;i<18;++i) assert(rgb[i]==255);
    for(int i=18;i<27;++i) assert(rgb[i]==76);
    u[0]=90; v[0]=240; y[0]=81;
    assert(ConvertYuv(3,3,planes,rgb)); assert(rgb[0]==255 && rgb[1]==0 && rgb[2]==0);
    const size_t old_size=rgb.size();
    assert(!ConvertYuv(0,3,planes,rgb)); assert(!ConvertYuv(-1,3,planes,rgb));
    assert(!ConvertYuv(INT_MAX,INT_MAX,planes,rgb)); assert(rgb.size()==old_size);
    planes[1].length=8; assert(!ConvertYuv(3,3,planes,rgb)); planes[1].length=12;
    planes[2].data=0; assert(!ConvertYuv(3,3,planes,rgb)); planes[2].data=v;
    planes[0].pixel_stride=0; assert(!ConvertYuv(3,3,planes,rgb)); planes[0].pixel_stride=2;
    planes[0].row_stride=4; assert(!ConvertYuv(3,3,planes,rgb)); planes[0].row_stride=8;
    planes[0].length=-1; assert(!ConvertYuv(3,3,planes,rgb));
    puts("PASS YUV: strided odd image, colors, padding, null/size/stride/length rejection");

    HandleRegistry<int,16> registry; registry.Reset(1);
    std::string first=registry.Insert(7), second=registry.Insert(9);
    assert(first!=second && *registry.Find(first)==7 && *registry.Find(second)==9);
    assert(registry.Erase(first) && !registry.Find(first) && *registry.Find(second)==9);
    assert(!registry.Erase(first));
    std::string replacement=registry.Insert(11); assert(replacement!=first);
    while(registry.Size()<16) assert(!registry.Insert(5).empty());
    assert(registry.Insert(5).empty());
    registry.Reset(2); assert(!registry.Find(second) && !registry.Find(replacement));
    assert(registry.Insert(7)!=first);
    puts("PASS handles: independent anchors, slot reuse, stale session, 16-entry exhaustion");
    assert(!ImageReady(300000,0)); assert(ImageReady(250000,1)); assert(!ImageReady(250001,1)); assert(!ImageReady(2,3));
    for (int height=720;height<=1280;height+=560) {
        assert(TopLeftY(0,height)==height); assert(TopLeftY((float)height,height)==0);
        assert(TopLeftY(123,height)==height-123);
    }
    puts("PASS 250ms stall boundary and bottom-left to top-left pixel coordinates");
    return 0;
}
