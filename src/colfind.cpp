// Disable min and max macros to avoid compiler error
#define NOMINMAX

// Windows headers
#include <windows.h>
#include <commdlg.h>

// Standard C++ library headers
#include <vector>   // For memory storage such as the list of loaded images in a particular window
#include <string>   // Well, for strings
#include <fstream>  // For file I/O

// Constants
#define THUMBNAIL_BASE_SIZE 500

// Win32 object IDs
#define ID_FILE_OPEN 1000

// Restore missing min and max features
template <typename T>
inline T min(T a, T b) {
    return (a < b) ? a : b;
}

template <typename T>
inline T max(T a, T b) {
    return (a > b) ? a : b;
}

// Struct to hold state information for a specific application window
struct WindowState {
    bool bInitialized;
    HWND hwnd;          // Window this state belongs to
    RECT clientRect;
    void* pBackbufferBits;
    HDC hdcBackbuffer;
    HBITMAP hBackbufferBitmap;

    WindowState() :
        bInitialized(false),
        hwnd(NULL),
        clientRect(),
        pBackbufferBits(NULL),
        hdcBackbuffer(NULL) {}
};

// Struct to hold image data
struct ImageData {
    std::string filename;
    BYTE* originalData;
    BYTE* processedData;
    HDC hdcMemOriginal;
    HBITMAP hOriginalBitmap;
    HDC hdcMemProcessed;
    HBITMAP hProcessedBitmap;
    int width;
    int height;
    std::vector<int> detectedColumns; // Store detected columns here
    HWND hwndVertLenEntry;


    ImageData() :
        originalData(NULL),
        processedData(NULL),
        width(0),
        height(0),
        hwndVertLenEntry(NULL) {}

    // Destructor to free allocated memory
    ~ImageData() {
        delete[] originalData;
        delete[] processedData;
    }

    // Copy constructor for deep copy
    ImageData(const ImageData& other)
        : width(other.width), height(other.height), detectedColumns(other.detectedColumns)
    {
        originalData = new BYTE[width * height * 4];
        processedData = new BYTE[width * height * 4];

        memcpy(originalData, other.originalData, width * height * 4);
        memcpy(processedData, other.processedData, width * height * 4);
    }

    // Copy assignment operator for deep copy
    ImageData& operator=(const ImageData& other) {
        if (this == &other) return *this;  // handle self-assignment

        delete[] originalData;
        delete[] processedData;

        width = other.width;
        height = other.height;
        detectedColumns = other.detectedColumns; // Copy detected columns

        originalData = new BYTE[width * height * 4];
        processedData = new BYTE[width * height * 4];

        memcpy(originalData, other.originalData, width * height * 4);
        memcpy(processedData, other.processedData, width * height * 4);

        return *this;
    }
};


// Forward declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void ProcessImage(const char* filename);
void RenderThumbnails(HWND hwnd, HDC hdc);

// Global variables
std::vector<ImageData> images;
float thumbnailScale = 1.0;
int thumbnailSpacing = 10;

// Helper function to create a DIB section
HBITMAP CreateDIBSection(HDC hdc, int width, int height, void** ppvBits)
{
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(BITMAPINFO));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;  // Top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    return CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, ppvBits, NULL, 0);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /* hPrevInstance */, LPSTR /* lpCmdLine */, int nCmdShow)
{
    // Register window class
    const char CLASS_NAME[] = "ImageProcessor";
    WNDCLASS wc;
    ZeroMemory(&wc, sizeof(WNDCLASS));
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    // Create window
    HWND hwnd = CreateWindow(
        CLASS_NAME, "Image Processor",
        WS_OVERLAPPEDWINDOW | WS_VSCROLL,
        CW_USEDEFAULT, CW_USEDEFAULT, 1600, 1000,
        NULL, NULL, hInstance, NULL
    );

    // Stack allocation is okay for this state object since WinMain stays
    // in the message pump until we're ready to quit. This stores state
    // information for the specific window it's attached to.
    WindowState state;

    // Store pointer to state object for this window as the user data
    // index. This will allow us to have multiple windows, which we couldn't
    // if we used static or global variables for this state information.
    SetWindowLong(hwnd, GWL_USERDATA, (long)(void*)&state);

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        {
            HMENU hMenu = CreateMenu();
            HMENU hSubMenu = CreatePopupMenu();
            if(!AppendMenu(hSubMenu, MF_STRING, ID_FILE_OPEN, "Open")) {
                MessageBox(hwnd, "Open item could not be added correctly. ", "Error", MB_OK);
            }
            AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hSubMenu, "File");
            SetMenu(hwnd, hMenu);

            // Register the window for drag-and-drop
            DragAcceptFiles(hwnd, TRUE);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        //RECT rect;

        //GetWindowRect(hwnd, &rect);
        //SelectObject(hdc, GetStockObject(WHITE_BRUSH));
        //Rectangle(hdc, 0, 0, rect.right, rect.bottom);

        RenderThumbnails(hwnd, hdc);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_COMMAND:
        switch (wParam)
        {
        case ID_FILE_OPEN:
            OPENFILENAME ofn;       // common dialog box structure
            char szFile[260];       // buffer for file name

            // Initialize OPENFILENAME
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = szFile;
            // Set lpstrFile[0] to '\0' so that GetOpenFileName does not
            // use the contents of szFile to initialize itself.
            ofn.lpstrFile[0] = '\0';
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = "All\0*.*\0Text\0*.TXT\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrFileTitle = NULL;
            ofn.nMaxFileTitle = 0;
            ofn.lpstrInitialDir = NULL;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            // Display the Open dialog box.
            if (GetOpenFileName(&ofn) == TRUE) {
                // Process file if one was selected
                ProcessImage(ofn.lpstrFile);
            }

            InvalidateRect(hwnd, NULL, TRUE);

            break;
        default:
            MessageBox(hwnd, "Invalid command ID encountered.", "Error", MB_OK);
        }
        return 0;
        case WM_MOUSEWHEEL:
        {
            // The wheel delta is in wParam. A positive delta means scrolling up, negative means scrolling down.
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);

            // Adjust thumbnail size (zoom in or out)
            if (delta > 0)
            {
                // Scrolled up, zoom in
                thumbnailScale = min(thumbnailScale + 0.1f, 100.0f); // Limit max thumbnail size to 500
            }
            else if (delta < 0)
            {
                // Scrolled down, zoom out
                thumbnailScale = max(thumbnailScale - 0.1f, 0.5f);  // Limit min thumbnail size to 50
            }

            // Invalidate the window to trigger a repaint with the new thumbnail size
            InvalidateRect(hwnd, NULL, TRUE);

            return 0;
        }
    case WM_DROPFILES:
        {
            HDROP hDrop = (HDROP)wParam;
            UINT fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
            for (UINT i = 0; i < fileCount; ++i)
            {
                char filename[MAX_PATH];
                DragQueryFile(hDrop, i, filename, MAX_PATH);
                ProcessImage(filename);
            }
            DragFinish(hDrop);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    case WM_SIZE:
    {
        // Invalidate the client area to force a redraw on resizing
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    case WM_VSCROLL:
        {
            SCROLLINFO si;
            si.cbSize = sizeof(SCROLLINFO);
            si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &si);

            switch (LOWORD(wParam))
            {
            case SB_TOP: si.nPos = si.nMin; break;
            case SB_BOTTOM: si.nPos = si.nMax; break;
            case SB_LINEUP: si.nPos -= 10; break;
            case SB_LINEDOWN: si.nPos += 10; break;
            case SB_PAGEUP: si.nPos -= si.nPage; break;
            case SB_PAGEDOWN: si.nPos += si.nPage; break;
            case SB_THUMBTRACK: si.nPos = si.nTrackPos; break;
            }

            si.fMask = SIF_POS;
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void SaveColumnDataToXML(const std::vector<int>& columnPositions, const char* filename)
{
    std::ofstream xmlFile;
    xmlFile.open(filename);

    if (!xmlFile.is_open()) {
        MessageBox(NULL, "Error opening XML file for writing.", "Error", MB_OK);
        return;
    }

    xmlFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xmlFile << "<ImageColumns>\n";

    for (size_t i = 0; i < columnPositions.size(); ++i) {
        xmlFile << "  <Column>" << columnPositions[i] << "</Column>\n";
    }

    xmlFile << "</ImageColumns>\n";
    xmlFile.close();
}

int bgrToGrayscale(int green, int blue, int red) {
    // Convert to grayscale using luminosity method
    return static_cast<int>(0.299 * red + 0.587 * green + 0.114 * blue);
}

void ProcessImage(const char* filename)
{
    HBITMAP hBitmap = (HBITMAP)LoadImage(NULL, filename, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    if (!hBitmap) return;

    BITMAP bm;
    GetObject(hBitmap, sizeof(BITMAP), &bm);

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    ImageData imgData;
    imgData.width = bm.bmWidth;
    imgData.height = bm.bmHeight;

    imgData.originalData = new BYTE[imgData.width * imgData.height * 4];
    imgData.processedData = new BYTE[imgData.width * imgData.height * 4];

    // Copy original image data into originalData buffer
    for (int y = 0; y < imgData.height; ++y) {
        for (int x = 0; x < imgData.width; ++x) {
            COLORREF color = GetPixel(hdcMem, x, y);
            int index = (y * imgData.width + x) * 4;
            imgData.originalData[index] = GetBValue(color);         // Blue
            imgData.originalData[index + 1] = GetGValue(color);     // Green
            imgData.originalData[index + 2] = GetRValue(color);     // Red
            imgData.originalData[index + 3] = 255;                  // Alpha

            // Convert to greyscale and set in the processedData array
            int grey = bgrToGrayscale(
                imgData.originalData[index],
                imgData.originalData[index+1],
                imgData.originalData[index+2]);
            imgData.processedData[index] = grey;                    // Blue
            imgData.processedData[index + 1] = grey;                // Green
            imgData.processedData[index + 2] = grey;                // Red
            imgData.processedData[index + 3] = 255;                 // Alpha
        }
    }

    // Step 1: Vertical Smearing (Simple Copying Down the Column)
    #define MAX_VERT 40
    for (int x = 0; x < imgData.width; ++x) {
        for (int y = 1; y < imgData.height; ++y) {
            int currIndex = (y * imgData.width + x) * 4;

            for (int vert = 1; vert <= min(y,MAX_VERT); ++vert) {
                int prevIndex = ((y - vert) * imgData.width + x) * 4;
                imgData.processedData[currIndex] /= 2;
                imgData.processedData[currIndex + 1] /= 2;
                imgData.processedData[currIndex + 2] /= 2;

                imgData.processedData[currIndex] += imgData.processedData[prevIndex] / 2;             // Blue
                imgData.processedData[currIndex + 1] += imgData.processedData[prevIndex + 1] / 2;     // Green
                imgData.processedData[currIndex + 2] += imgData.processedData[prevIndex + 2] / 2;     // Red
                imgData.processedData[currIndex + 3] = 255;                                     // Alpha
            }
        }
    }

    // Step 2: Column/Line Detection (Simple Edge Detection in Grayscale)
    int threshold = 20;
    for (int y = 0; y < imgData.height; ++y) {
        for (int x = 1; x < imgData.width - 1; ++x) {
            int currIndex = (y * imgData.width + x) * 4;
            int leftIndex = (y * imgData.width + (x - 1)) * 4;
            int rightIndex = (y * imgData.width + (x + 1)) * 4;

            BYTE grayCurr = (imgData.originalData[currIndex] + imgData.originalData[currIndex + 1] + imgData.originalData[currIndex + 2]) / 3;
            BYTE grayLeft = (imgData.originalData[leftIndex] + imgData.originalData[leftIndex + 1] + imgData.originalData[leftIndex + 2]) / 3;
            BYTE grayRight = (imgData.originalData[rightIndex] + imgData.originalData[rightIndex + 1] + imgData.originalData[rightIndex + 2]) / 3;

            if (abs(grayCurr - grayLeft) > threshold || abs(grayCurr - grayRight) > threshold) {
                imgData.detectedColumns.push_back(x);  // Save detected column
                break;
            }
        }
    }

    SelectObject(hdcMem, hOldBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    DeleteObject(hBitmap);

    // Add parameter adjustment controls


    // Store image data via copy constructor
    images.push_back(imgData);
}

HBITMAP BitsToThumbnailBitmap(HDC hdc, int sourceWidth, int sourceHeight, BYTE* bytes)
{
        void* pBits;
        HBITMAP hBitmap = CreateDIBSection(hdc, THUMBNAIL_BASE_SIZE, THUMBNAIL_BASE_SIZE, &pBits);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdc, hBitmap);

        // Copy image data to bits array, while scaling image data to THUMBNAIL_BASE_SIZE (for
        // both width and height) from the original file size
        for (int ty = 0; ty < THUMBNAIL_BASE_SIZE; ++ty) {      // Target y = ty
            for (int tx = 0; tx < THUMBNAIL_BASE_SIZE; ++tx) {  // Target x = tx
                // Source sx,sy = target thumbnail position tx,ty; scaled to source size
                int sx = tx * sourceWidth / THUMBNAIL_BASE_SIZE;
                int sy = ty * sourceHeight / THUMBNAIL_BASE_SIZE;
                // Scaled destination index (tx,ty converted to linear index in array)
                int sIndex = (sy * sourceWidth + sx) * 4;
                int tIndex = (ty * THUMBNAIL_BASE_SIZE + tx) * 4;
                ((BYTE*)pBits)[tIndex] = bytes[sIndex];
                ((BYTE*)pBits)[tIndex + 1] = bytes[sIndex + 1];
                ((BYTE*)pBits)[tIndex + 2] = bytes[sIndex + 2];
                ((BYTE*)pBits)[tIndex + 3] = bytes[sIndex + 3];
            }
        }

        return hBitmap;
}

void RenderThumbnails(HWND hwnd, HDC hdc)
{
    WindowState* win;
    win = (WindowState*)GetWindowLong(hwnd, GWL_USERDATA);
    if(win == NULL) {
        MessageBox(hwnd, "Internal state error, no user data pointer found on HWND. The application must exit.", "Error", MB_OK | MB_ICONEXCLAMATION);
        exit(9);
    }

    GetClientRect(hwnd, &(win->clientRect));

    if(!win->bInitialized) {
        win->hdcBackbuffer = CreateCompatibleDC(hdc);
        win->hBackbufferBitmap = CreateDIBSection(
            win->hdcBackbuffer,
            win->clientRect.right,
            win->clientRect.bottom,
            &win->pBackbufferBits
        );
        SelectObject(win->hdcBackbuffer, win->hBackbufferBitmap);
    }

    // Clear backbuffer
    SelectObject(win->hdcBackbuffer, GetStockObject(WHITE_BRUSH));
    Rectangle(win->hdcBackbuffer, -1, -1, win->clientRect.right+1, win->clientRect.bottom+1);


    // Get scroll pos
    SCROLLINFO si;
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_ALL;
    GetScrollInfo(hwnd, SB_VERT, &si);

    int xPos = thumbnailSpacing;
    int yPos = thumbnailSpacing - si.nPos;
    int thumbnailSize = THUMBNAIL_BASE_SIZE * thumbnailScale;

    for (size_t i = 0; i < images.size(); ++i)
    {
        ImageData& img = images[i];

        // Create memory DC and bitmap sections for original and processed image data
        if(img.hdcMemOriginal == NULL) img.hdcMemOriginal = CreateCompatibleDC(hdc);
        if(img.hOriginalBitmap == NULL)
            img.hOriginalBitmap = BitsToThumbnailBitmap(img.hdcMemOriginal, img.width, img.height, img.originalData);

        if(img.hdcMemProcessed == NULL) img.hdcMemProcessed = CreateCompatibleDC(hdc);
        if(img.hProcessedBitmap == NULL)
            img.hProcessedBitmap = BitsToThumbnailBitmap(img.hdcMemProcessed, img.width, img.height, img.processedData);

        //===================================================================//
        // Render original thumbnail
        StretchBlt(
            win->hdcBackbuffer,     // hdcDest
            xPos,                   // xDest
            yPos,                   // yDest
            thumbnailSize,          // wDest
            thumbnailSize,          // hDest
            img.hdcMemOriginal,     // hdcSrc
            0,                      // xSrc
            0,                      // ySrc
            THUMBNAIL_BASE_SIZE,    // wSrc
            THUMBNAIL_BASE_SIZE,    // hSrc
            SRCCOPY
        );

        /*
        // Renders unscaled thumbnail?
        BitBlt(
            hdcBackbuffer,          // hdcDest
            xPos,                   // xDest
            yPos,                   // yDest
            thumbnailSize,          // wDest
            thumbnailSize,          // hDest
            img.hdcMem,             // hdcSrc
            0,0, SRCCOPY);
        */

        // FPO / debug cross line
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));  // Red for original
        HPEN hOldPen = (HPEN)SelectObject(win->hdcBackbuffer, hPen);
        MoveToEx(win->hdcBackbuffer, xPos, yPos, NULL);
        LineTo(win->hdcBackbuffer, xPos + thumbnailSize, yPos + thumbnailSize);
        SelectObject(win->hdcBackbuffer, hOldPen);
        DeleteObject(hPen);


        // Position next thumbnail
        MoveToEx(win->hdcBackbuffer, xPos, yPos, NULL);

        xPos += thumbnailSize + thumbnailSpacing;
        if (xPos + thumbnailSize > win->clientRect.right) {
            xPos = thumbnailSpacing;
            yPos += (thumbnailSize + thumbnailSpacing);
        }

        //===================================================================//
        // Render processed thumbnail
        StretchBlt(
            win->hdcBackbuffer,          // hdcDest
            xPos,                   // xDest
            yPos,                   // yDest
            thumbnailSize,          // wDest
            thumbnailSize,          // hDest
            img.hdcMemProcessed,    // hdcSrc
            0,                      // xSrc
            0,                      // ySrc
            THUMBNAIL_BASE_SIZE,    // wSrc
            THUMBNAIL_BASE_SIZE,    // hSrc
            SRCCOPY
        );


        // FPO / debug cross line
        hPen = CreatePen(PS_SOLID, 1, RGB(0, 255, 0));  // Green for processed
        hOldPen = (HPEN)SelectObject(win->hdcBackbuffer, hPen);
        MoveToEx(win->hdcBackbuffer, xPos, yPos, NULL);
        LineTo(win->hdcBackbuffer, xPos + thumbnailSize, yPos + thumbnailSize);
        SelectObject(win->hdcBackbuffer, hOldPen);
        DeleteObject(hPen);


        // Draw column frames
        hPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 255));              // Blue for column
        HPEN hRedPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));      // Red for top-left corner of column
        HPEN hPurplePen = CreatePen(PS_SOLID, 1, RGB(128, 0, 128));   // Purple for bottom-right of column

        hOldPen = (HPEN)SelectObject(win->hdcBackbuffer, hPen);

        int xOrigin, yOrigin;
        for(int col = 0; col < img.detectedColumns.size()-1; col += 2) {

            // ========== Draw PURPLE bottom-right corner lines ========== //
            SelectObject(win->hdcBackbuffer, hPurplePen);

            // Origin for bottom-right corner, from column `col` to column `col+1`
            xOrigin = xPos + img.detectedColumns[col] * thumbnailScale
                           + img.detectedColumns[col+1] * thumbnailScale;
            yOrigin = yPos + thumbnailSize;

            // Bottom line
            MoveToEx(win->hdcBackbuffer, xOrigin, yOrigin, NULL);
            LineTo(win->hdcBackbuffer, xOrigin - 10 * thumbnailScale, yOrigin);

            // Right line
            MoveToEx(win->hdcBackbuffer, xOrigin, yOrigin, NULL);
            LineTo(win->hdcBackbuffer, xOrigin, yOrigin - 10 * thumbnailScale);

            // Origin for top-left corner -- shared between corner lines and diagonal cross line
            xOrigin = xPos + img.detectedColumns[col] * thumbnailScale;
            yOrigin = yPos;


            // ========== Draw RED top-left corner lines ========== //
            SelectObject(win->hdcBackbuffer, hRedPen);

            // Top line
            MoveToEx(win->hdcBackbuffer,    xOrigin,                        yOrigin, NULL);
            LineTo(win->hdcBackbuffer,      xOrigin + 10 * thumbnailScale,  yOrigin);

            // Left line
            MoveToEx(win->hdcBackbuffer,    xOrigin,                        yOrigin, NULL);
            LineTo(win->hdcBackbuffer,      xOrigin,                        yOrigin + 10 * thumbnailScale);

            // ========== Draw BLUE diagonal line across column ========== //
            SelectObject(win->hdcBackbuffer, hPen);

            MoveToEx(win->hdcBackbuffer,    xOrigin,                        yOrigin, NULL);
            LineTo(win->hdcBackbuffer,      xOrigin + img.detectedColumns[col+1]
                                                    * thumbnailScale,       yOrigin + thumbnailSize);
        }
        SelectObject(win->hdcBackbuffer, hOldPen);
        DeleteObject(hPen);



        // Position next thumbnail
        xPos += thumbnailSize + thumbnailSpacing;
        if (xPos + thumbnailSize > win->clientRect.right) {
            xPos = thumbnailSpacing;
            yPos += (thumbnailSize + thumbnailSpacing);
        }

    }

    // Render backbuffer to window
    BitBlt(
        hdc,                        // hdcDest
        0,                          // xDest
        0,                          // yDest
        win->clientRect.right,           // wDest
        win->clientRect.bottom,          // hDest
        win->hdcBackbuffer,              // hdcSrc
        0,                          // xSrc
        0,                          // ySrc
        SRCCOPY
    );

    // Update scrollbar
    si.fMask = SIF_RANGE | SIF_PAGE;
    si.nMin = 0;
    si.nMax = yPos + thumbnailSize * thumbnailScale;
    si.nPage = win->clientRect.bottom;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

}
