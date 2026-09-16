// plane_probe.c — read-only: read display/panel regs via MyIntelGPU ReadMMIO (selector 6)
#include <IOKit/IOKitLib.h>
#include <stdio.h>
#include <mach/mach.h>
static uint32_t rd(io_connect_t c, uint32_t r){uint64_t o=0,i=r;uint32_t n=1;IOConnectCallScalarMethod(c,6,&i,1,&o,&n);return(uint32_t)o;}
int main(void){
    io_service_t s=IOServiceGetMatchingService(kIOMainPortDefault,IOServiceMatching("MyIntelGPU"));
    if(!s){fprintf(stderr,"FAIL: MyIntelGPU service not found\n");return 1;}
    io_connect_t c=0;IOServiceOpen(s,mach_task_self(),0,&c);
    uint32_t ctl=rd(c,0x70180),stride=rd(c,0x70188),size=rd(c,0x70190),surf=rd(c,0x7019C),live=rd(c,0x701AC),pipe=rd(c,0x70080);
    printf("CTL=0x%08X STRIDE=0x%08X SIZE=0x%08X SURF=0x%08X LIVE=0x%08X PIPE=0x%08X\n",ctl,stride,size,surf,live,pipe);
    printf("plane en: %s | surf==live: %s (0x%X vs 0x%X)\n",(ctl&0x80000000u)?"YES":"NO",(surf==live)?"YES":"NO",surf,live);
    IOServiceClose(c);return 0;}