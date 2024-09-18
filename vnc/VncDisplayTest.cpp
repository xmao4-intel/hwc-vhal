#include "VncDisplay.h"

#if 1
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    VncDisplay* vd = new VncDisplay(9000, 640, 480);
    if (!vd) {
        printf("Faild to create VncDisplay\n");
        return -1;
    }

    if (vd->init() < 0) {
        printf("Failed to init VncDisplay\n");
        return -1;
    }

    int color = 0;
    while (1) {
        vd->fillColor(color += 0x123456);
        usleep(200000);
    }
    return 0;
}

#else
int main(int argc,char** argv)
{
  rfbScreenInfoPtr server=rfbGetScreen(&argc,argv,400,300,8,3,4);
  if(!server)
    return 0;
  server->port = 9000;
  server->frameBuffer=(char*)malloc(400*300*4);
  memset(server->frameBuffer, 0x40, 400*300*4);
  rfbInitServer(server);
  rfbRunEventLoop(server,-1,FALSE);
  return(0);
}
#endif
