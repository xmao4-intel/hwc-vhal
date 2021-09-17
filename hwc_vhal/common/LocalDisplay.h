#ifndef __LOCAL_DISPLAY_H__
#define __LOCAL_DISPLAY_H__

int getResFromFb(int& w, int& h);
int getResFromDebugFs(int& w, int& h);
int getResFromKms(int& w, int& h);

#endif