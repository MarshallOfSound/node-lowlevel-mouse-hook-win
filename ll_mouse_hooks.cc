#include <nan.h>
#include <sstream>
#include <string>
#include <iostream>
#include <stdlib.h>
#include <Windows.h>
#define _WIN32_WINNT 0x050

using namespace v8;

static Persistent<Function> persistentCallback;
HHOOK hhkLowLevelKybd;
uv_loop_t *loop;
uv_async_t async;
std::string str;
int running = 0;

void stop() {
    if (hhkLowLevelKybd) {
        UnhookWindowsHookEx(hhkLowLevelKybd);
    }
    
    uv_close((uv_handle_t*)&async, NULL);
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    MOUSEHOOKSTRUCT * pMouseStruct = (MOUSEHOOKSTRUCT *)lParam;
    if (pMouseStruct != NULL) {
        int x = pMouseStruct->pt.x;
        int y = pMouseStruct->pt.y;
        std::ostringstream streamX;
        std::ostringstream streamY;
        streamX << x;
        streamY << y;
        str = "move::" + streamX.str() + "::" + streamY.str();
        async.data = &str;
        uv_async_send(&async);
    }
    switch (wParam) {
        case WM_LBUTTONDOWN:
            str = "down";
            async.data = &str;
            uv_async_send(&async);
            return 1;
        case WM_LBUTTONUP:
            str = "up";
            async.data = &str;
            uv_async_send(&async);
            return 1;
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void hook() {
    printf("Hooking\n");
  hhkLowLevelKybd = SetWindowsHookEx(WH_MOUSE_LL, LowLevelKeyboardProc, 0, 0);

  MSG msg;
  while (!GetMessage(&msg, NULL, NULL, NULL)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
  }

   UnhookWindowsHookEx(hhkLowLevelKybd);
}

void handleKeyEvent(uv_async_t *handle) {
    std::string &keyCodeString = *(static_cast<std::string*>(handle->data));

    const unsigned argc = 1;
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    Local<Value> argv[argc] = { String::NewFromUtf8(isolate, keyCodeString.c_str()) };

    Local<Function> f = Local<Function>::New(isolate,persistentCallback);
    f->Call(isolate->GetCurrentContext()->Global(), argc, argv);

    if (keyCodeString == "up") {
        uv_close((uv_handle_t*)&async, NULL);
        running = 0;
    }
}

uv_thread_t t_id;

void RunCallback(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();

    HandleScope scope(isolate);

    Handle<Function> cb = Handle<Function>::Cast(args[0]);
    persistentCallback.Reset(isolate, cb);

    if (running == 1) {
        uv_close((uv_handle_t*)&async, NULL);
        running = 0;
    }

    loop = uv_default_loop();

    uv_work_t req;

    int param = 0;
    uv_thread_cb uvcb = (uv_thread_cb)hook;
    uv_async_init(loop, &async, handleKeyEvent);

    uv_thread_create(&t_id, uvcb, &param);
    running = 1;
}

void StopCallback(const FunctionCallbackInfo<Value>& args) {
    if (running == 1) {
        uv_close((uv_handle_t*)&async, NULL);
        running = 0;
    }
}

void Init(Handle<Object> exports, Handle<Object> module) {
  NODE_SET_METHOD(exports, "run", RunCallback);
  NODE_SET_METHOD(exports, "stop", StopCallback);
}

NODE_MODULE(addon, Init)
