#include <thread>
#include <string>
#include <vector>
#include <fstream>

#include "framework.h"
#include "screen_buddy.h"

#include <objidl.h> 
#include <shellapi.h>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")

#define MAX_LOADSTRING 100
#define WM_WORKER_START  (WM_USER + 1)
#define WM_WORKER_FINISH (WM_USER + 2)
#define null NULL


typedef bool (*crypto_init_func)(const unsigned char*, size_t);
typedef bool (*process_buffer_func)(const unsigned char*, unsigned char*, size_t, bool);
typedef void (*crypto_clean_func)();

// Глобальные переменные для загрузки DLL
HMODULE hPlugin = nullptr;
crypto_init_func pcrypto_init = nullptr;
process_buffer_func pprocess_buffer = nullptr;
crypto_clean_func pcrypto_clean = nullptr;

// Глобальные переменные:
HINSTANCE hInst;                                // текущий экземпляр
WCHAR szTitle[MAX_LOADSTRING];                  // Текст строки заголовка
WCHAR szWindowClass[MAX_LOADSTRING];            // имя класса главного окна

int HIGHT = 300;
int WIDTH = 300; 

bool is_processing = false;

unsigned int count_of_file = 0; //костыль
size_t buffer_size = 4096;//что бы можно было менять, наверное настройки сделаю ещё

// Отправить объявления функций, включенных в этот модуль кода:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

bool save_key_to_register(const std::vector<unsigned char>& key){
   HKEY secret_key;
   //key.data(),key.size()
   LSTATUS status = RegCreateKeyExW(
       HKEY_CURRENT_USER, L"Software\\screen_buddy", 0, nullptr,
       REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &secret_key,
       nullptr
   );

   if (status == ERROR_SUCCESS) {
    RegSetValueExW(
        secret_key, L"secret_key", 0,REG_BINARY,            
        (const BYTE*)key.data(), (DWORD)key.size()
    );

    
    RegCloseKey(secret_key);
   }

   if (status == ERROR_SUCCESS){
       return true;
   }

   return false;
}

std::vector <unsigned char> load_key_from_register(){
    DWORD data_size = 0;

    LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER, L"Software\\screen_buddy", L"secret_key",
        RRF_RT_REG_BINARY, nullptr, nullptr, &data_size
    );

    std::vector <unsigned char> key(data_size);

    if (status == ERROR_SUCCESS) {

        status = RegGetValueW(
            HKEY_CURRENT_USER, L"Software\\screen_buddy", L"secret_key",
            RRF_RT_REG_BINARY, nullptr, key.data(), &data_size
        );
    }
    
    return key;
}


std::wstring parse_file_name(const std::wstring file_path){
    std::wstring file_name;
    auto it = file_path.rbegin();
    while(it != file_path.rend() && *it != L'\\' && *it != L'/'){
        file_name.push_back(*it);
        ++it;
    }
    std::reverse(file_name.begin(), file_name.end());
    return file_name;
}

void process_file_worker(HWND hWnd,std::wstring file_path){

    PostMessageW(hWnd, WM_WORKER_START, 0, 0);//начали

    std::wstring file_name = parse_file_name(file_path);
    std::wstring new_file_name =std::to_wstring(count_of_file++) + file_name;

    size_t pos = file_path.find_last_of(L"\\/");

    std::wstring out_path;
    if (pos != std::wstring::npos) {
        out_path = file_path.substr(0, pos + 1) + new_file_name;
    } else {
        out_path = new_file_name;
    }
    
    //MessageBox(hWnd,out_path.c_str(), L"Путь к зашифрованному файлу", MB_OK);
    std::ifstream in(file_path, std::ios::binary);
    std::ofstream out(out_path, std::ios::binary);
    //так-то они оба 100% должны открытся, но на всякий
    if(!in.is_open() || !out.is_open()){
        OutputDebugStringW(L"Файл не открывается");
        return;
    }


    auto key = load_key_from_register() ;//так нельзя делать
    if (key.empty()) {
        OutputDebugStringW(L"Ключ в реестре не найден!");
        return;
    }
    pcrypto_init(key.data(), key.size());//надо, наверное функцию генерации ключа сделать какую-то, либо из .dll подгружать генератор ключей 
    
    std::vector<unsigned char> read_buffer(buffer_size,0);
    std::vector<unsigned char> write_buffer(buffer_size,0);

    while(in){
        in.read((char*)read_buffer.data(), buffer_size);
        std::streamsize bytes_read = in.gcount(); 

        if (bytes_read>0){
            pprocess_buffer(read_buffer.data(), write_buffer.data(), bytes_read, true);
            out.write((char*)write_buffer.data(), bytes_read);
        }
    }

    pcrypto_clean(); 

    PostMessageW(hWnd, WM_WORKER_FINISH, 0, 0); //закончили
}


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    using namespace Gdiplus;
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Разместите код здесь.

    // Инициализация глобальных строк
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_SCREENBUDDY, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    GdiplusStartupInput start;
    ULONG_PTR gdiplus_token;

    GdiplusStartup(&gdiplus_token, &start, NULL);

    // Выполнить инициализацию приложения:
    if (!InitInstance (hInstance, nCmdShow)){
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SCREENBUDDY));

    MSG msg;

    // Цикл основного сообщения:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    GdiplusShutdown(gdiplus_token);
    return (int) msg.wParam;
}



//
//  ФУНКЦИЯ: MyRegisterClass()
//
//  ЦЕЛЬ: Регистрирует класс окна.
//
ATOM MyRegisterClass(HINSTANCE hInstance){
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SCREENBUDDY));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}


void updete_buddy_visuality(HWND hWnd, const WCHAR* image_path){
    using namespace Gdiplus;

    std::unique_ptr<Bitmap> bitmap(Bitmap::FromFile(image_path));
    if(bitmap && bitmap->GetLastStatus() == Ok){

        HDC hdc_screen = GetDC(null);
        HDC hdc_memory = CreateCompatibleDC(hdc_screen);



    }
}



//
//   ФУНКЦИЯ: InitInstance(HINSTANCE, int)
//
//   ЦЕЛЬ: Сохраняет маркер экземпляра и создает главное окно
//
//   КОММЕНТАРИИ:
//
//        В этой функции маркер экземпляра сохраняется в глобальной переменной, а также
//        создается и выводится главное окно программы.
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow){
   hInst = hInstance; // Сохранить маркер экземпляра в глобальной переменной

  /* HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);*/

   HWND hWnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_ACCEPTFILES, szWindowClass, szTitle, WS_POPUP, CW_USEDEFAULT, 0, HIGHT, WIDTH, nullptr, nullptr, hInstance, nullptr);

   //SetLayeredWindowAttributes(hWnd, 0, 128, LWA_ALPHA);

   if (!hWnd){
      return FALSE;
   }

   //загрузка .dll
   hPlugin = LoadLibraryW(L"encryption_algorithm.dll");
   if (hPlugin) {
       
       pcrypto_init = (crypto_init_func)GetProcAddress(hPlugin, "crypto_init");
       pprocess_buffer = (process_buffer_func)GetProcAddress(hPlugin, "process_buffer");
       pcrypto_clean = (crypto_clean_func)GetProcAddress(hPlugin, "crypto_clean");

       if (!pcrypto_init || !pprocess_buffer || !pcrypto_clean) {
           MessageBoxW(nullptr, L"Функции в DLL не найдены! Проверь extern \"C\" и экспорты.", L"Ошибка", MB_OK | MB_ICONERROR);
       }
   } else {
       MessageBoxW(nullptr, L"Не удалось найти encryption_algorithm.dll!", L"Ошибка", MB_OK | MB_ICONERROR);
   }
   //грузим ключик
   auto key = load_key_from_register();

   if (key.empty()) {
       key = { 0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF };
       save_key_to_register(key);
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  ФУНКЦИЯ: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  ЦЕЛЬ: Обрабатывает сообщения в главном окне.
//
//  WM_COMMAND  - обработать меню приложения
//  WM_PAINT    - Отрисовка главного окна
//  WM_DESTROY  - отправить сообщение о выходе и вернуться
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message){

    case WM_DROPFILES: {
        HDROP hDrop = (HDROP) wParam;

        if (is_processing) {
            MessageBoxW(hWnd, L"Дождитесь окончания текущей операции!", L"Занято", MB_OK | MB_ICONWARNING);
            DragFinish(hDrop);
            break; 
        }

        WCHAR filePath[MAX_PATH];

        if (DragQueryFileW(hDrop, 0, filePath, MAX_PATH)){
            MessageBoxW(hWnd, filePath, L"Файл получен!", MB_OK | MB_ICONINFORMATION);
            if(pcrypto_init && pprocess_buffer && pcrypto_clean){

                std::thread(process_file_worker, hWnd, std::wstring(filePath)).detach();
                //MessageBoxA(hWnd, (const char*)encrypted, "Зашифрованная строка:", MB_OK);
        
            }
            else {
                MessageBoxW(hWnd, L"DLL не загружена, шифрование невозможно.", L"Внимание", MB_OK);
            }
        }
        DragFinish(hDrop); 

    } break;
    
    case WM_RBUTTONUP: {
        DestroyWindow(hWnd);
    } break;
    
    case WM_NCHITTEST: {
        LRESULT hit = DefWindowProc(hWnd, message, wParam, lParam);
        if (hit == HTCLIENT) {
            return HTCAPTION;
        }
        return hit;
    } break;

    case WM_COMMAND:{
            int wmId = LOWORD(wParam);
            
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:{
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Добавьте сюда любой код прорисовки, использующий HDC...
            EndPaint(hWnd, &ps);
        }
        break;

    case WM_WORKER_START: {
        is_processing = true;
    } break;

    case WM_WORKER_FINISH: {
        is_processing = false;
        MessageBoxW(hWnd, L"Шифрование файла завершено!", L"Успех", MB_OK | MB_ICONINFORMATION);
    } break;

    case WM_DESTROY:{
        if (hPlugin) {
            FreeLibrary(hPlugin);
        }

        PostQuitMessage(0);
    }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Обработчик сообщений для окна "О программе".
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
