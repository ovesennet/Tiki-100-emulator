#include <windows.h>
#include <stdio.h>
LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){
  if(m==WM_KEYDOWN){printf("VK=0x%02X (%d)\n",(int)w,(int)w);fflush(stdout);}
  if(m==WM_DESTROY){PostQuitMessage(0);return 0;}
  return DefWindowProc(h,m,w,l);
}
int main(){
  WNDCLASS wc={0};MSG msg;
  wc.lpfnWndProc=WndProc;wc.hInstance=GetModuleHandle(0);wc.lpszClassName="K";
  RegisterClass(&wc);
  CreateWindow("K","Press keys - check console",WS_OVERLAPPEDWINDOW|WS_VISIBLE,100,100,400,200,0,0,wc.hInstance,0);
  while(GetMessage(&msg,0,0,0)){TranslateMessage(&msg);DispatchMessage(&msg);}
}
