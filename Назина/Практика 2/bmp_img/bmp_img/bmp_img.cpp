#include <windows.h>
#include <commdlg.h>
#include <string>
#include <fstream>
#include <vector>
#include <iostream>

using namespace std;

// Глобальные переменные
#define IDM_OPEN 1001
#define IDM_ABOUT 1002

// Структура для хранения информации об изображении
struct Изображение {
    std::vector<COLORREF>* пиксели;
    int высота, ширина;
    int фигураXМин, фигураXМакс, фигураYМин, фигураYМакс;
    COLORREF цветФона;
};

// Функция для открытия BMP файла
bool ОткрытьBMP(HWND hWnd, wchar_t** выбранныйФайл) {
    OPENFILENAME ofn;
    wchar_t имяФайла[MAX_PATH] = L"";

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = L"BMP Files\0*.bmp\0";
    ofn.lpstrFile = имяФайла;
    ofn.lpstrTitle = L"Выберите BMP файл";
    ofn.nMaxFile = sizeof(имяФайла);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        *выбранныйФайл = _wcsdup(имяФайла);
        return true;
    }
    return false;
}

// Проверка близости цветов с заданным допуском
bool ПохожиеЦвета(COLORREF цвет1, COLORREF цвет2, int допуск) {
    int r1 = GetRValue(цвет1), g1 = GetGValue(цвет1), b1 = GetBValue(цвет1);
    int r2 = GetRValue(цвет2), g2 = GetGValue(цвет2), b2 = GetBValue(цвет2);
    return (abs(r1 - r2) <= допуск && abs(g1 - g2) <= допуск && abs(b1 - b2) <= допуск);
}

// Чтение пикселей из BMP файла
Изображение* ЗагрузитьИзображение(const wchar_t* имяФайла) {
    ifstream файл(имяФайла, ios::binary);
    if (!файл.is_open()) return nullptr;

    char заголовок[54];
    файл.read(заголовок, 54);

    int ширина = *(int*)&заголовок[18];
    int высота = *(int*)&заголовок[22];
    int размерСтроки = ((ширина * 3 + 3) & ~3); // Учет выравнивания
    char* данныеСтроки = new char[размерСтроки];

    vector<COLORREF>* цветаПикселей = new vector<COLORREF>();
    COLORREF цветФона;

    for (int y = 0; y < высота; ++y) {
        файл.read(данныеСтроки, размерСтроки);
        for (int x = 0; x < ширина; ++x) {
            int i = x * 3;
            COLORREF цвет = RGB((unsigned char)данныеСтроки[i + 2], (unsigned char)данныеСтроки[i + 1], (unsigned char)данныеСтроки[i]);
            if (y == 0 && x == 0) цветФона = цвет; // Первый пиксель — цвет фона
            цветаПикселей->push_back(цвет);
        }
    }

    delete[] данныеСтроки;

    Изображение* изображение = new Изображение{ цветаПикселей, высота, ширина, INT_MAX, 0, INT_MAX, 0, цветФона };

    // Определение границ фигуры
    for (int y = 0; y < высота; ++y) {
        for (int x = 0; x < ширина; ++x) {
            COLORREF цвет = цветаПикселей->at(y * ширина + x);
            if (!ПохожиеЦвета(цвет, цветФона, 10)) {
                изображение->фигураXМин = min(изображение->фигураXМин, x);
                изображение->фигураXМакс = max(изображение->фигураXМакс, x);
                изображение->фигураYМин = min(изображение->фигураYМин, y);
                изображение->фигураYМакс = max(изображение->фигураYМакс, y);
            }
        }
    }

    return изображение;
}

// Отображение изображения в окне
void НарисоватьИзображение(const Изображение* изображение, HDC hdcWindow, HWND hwnd) {
    int ширинаФигуры = изображение->фигураXМакс - изображение->фигураXМин + 1;
    int высотаФигуры = изображение->фигураYМакс - изображение->фигураYМин + 1;

    // Заливка фона окна белым цветом
    HBRUSH кистьБелая = CreateSolidBrush(RGB(255, 255, 255));
    RECT размерОкна;
    GetClientRect(hwnd, &размерОкна);
    FillRect(hdcWindow, &размерОкна, кистьБелая);
    DeleteObject(кистьБелая);

    // Центрирование фигуры в верхней части окна
    int центрОкнаX = (размерОкна.right - размерОкна.left) / 2;
    int позицияX = центрОкнаX - ширинаФигуры / 2;
    int позицияY = 0;

    for (int x = 0; x < ширинаФигуры; ++x) {
        for (int y = 0; y < высотаФигуры; ++y) {
            int перевернутыйX = x;
            int перевернутыйY = высотаФигуры - 1 - y; // Отзеркаливание по вертикали

            COLORREF цвет = изображение->пиксели->at((изображение->фигураYМин + перевернутыйY) * изображение->ширина + изображение->фигураXМин + перевернутыйX);
            if (!ПохожиеЦвета(цвет, изображение->цветФона, 10)) {
                SetPixel(hdcWindow, позицияX + x, позицияY + y, цвет);
            }
        }
    }
}

// Обработчик сообщений окна
LRESULT CALLBACK ОбработчикСообщений(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static wchar_t* выбранныйФайл = nullptr;
    static Изображение* изображение = nullptr;

    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == IDM_OPEN) {
            if (ОткрытьBMP(hwnd, &выбранныйФайл)) {
                if (изображение) delete изображение;
                изображение = ЗагрузитьИзображение(выбранныйФайл);
                if (изображение) {
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
        }
        else if (LOWORD(wParam) == IDM_ABOUT) {
            MessageBox(hwnd, L"Выполнила Назина Д.С., гр. НМТ-323901 Вариант 9", L"О программе", MB_OK);
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (изображение) {
            НарисоватьИзображение(изображение, hdc, hwnd);
        }
        EndPaint(hwnd, &ps);
        break;
    }

    case WM_CLOSE:
        if (изображение) delete изображение;
        PostQuitMessage(0);
        break;

    case WM_SIZE:
        InvalidateRect(hwnd, NULL, TRUE);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Точка входа в программу
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t ИМЯ_КЛАССА[] = L"ОкноПрограммы";

    WNDCLASS wc = {};
    wc.lpfnWndProc = ОбработчикСообщений;
    wc.hInstance = hInstance;
    wc.lpszClassName = ИМЯ_КЛАССА;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        ИМЯ_КЛАССА,
        L"Просмотр BMP изображений",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) return 0;

    // Создание меню
    HMENU меню = CreateMenu();
    HMENU подМеню = CreatePopupMenu();
    AppendMenu(подМеню, MF_STRING, IDM_OPEN, L"Открыть BMP файл");
    AppendMenu(меню, MF_STRING | MF_POPUP, (UINT_PTR)подМеню, L"Файл");
    AppendMenu(меню, MF_STRING, IDM_ABOUT, L"О программе");
    SetMenu(hwnd, меню);

    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
