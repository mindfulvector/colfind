
#define NOMINMAX

#include <windows.h>
#include <commdlg.h>
#include <vector>
#include <string>
#include <fstream>  // For file I/O

#define THUMBNAIL_BASE_SIZE 500
#define ID_FILE_OPEN 1000

template <typename T>
inline T min(T a, T b) {
    return (a < b) ? a : b;
}

template <typename T>
inline T max(T a, T b) {
    return (a > b) ? a : b;
}

// Struct to hold image data
struct ImageData {
    std::string filename;
    BYTE* originalData;
    BYTE* processedData;
    HDC hdcMem;
    HBITMAP hOriginalBitmap;
    HBITMAP hProcessedBitmap;
    int width;
    int height;
    std::vector<int> detectedColumns; // Store detected columns here

    ImageData() : originalData(NULL), processedData(NULL), width(0), height(0) {}

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
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL
    );

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
        RECT rect;

        GetWindowRect(hwnd, &rect);
        SelectObject(hdc, GetStockObject(WHITE_BRUSH));
        Rectangle(hdc, 0, 0, rect.right, rect.bottom);

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
                thumbnailScale = min(thumbnailScale + 0.01f, 100.0f); // Limit max thumbnail size to 500
            }
            else if (delta < 0)
            {
                // Scrolled down, zoom out
                thumbnailScale = max(thumbnailScale - 0.01f, 0.5f);  // Limit min thumbnail size to 50
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
            imgData.originalData[index] = GetRValue(color);
            imgData.originalData[index + 1] = GetGValue(color);
            imgData.originalData[index + 2] = GetBValue(color);
            imgData.originalData[index + 3] = 255; // Alpha
        }
    }

    // Step 1: Vertical Smearing (Simple Copying Down the Column)
    for (int x = 0; x < imgData.width; ++x) {
        for (int y = 1; y < imgData.height; ++y) {
            int prevIndex = ((y - 1) * imgData.width + x) * 4;
            int currIndex = (y * imgData.width + x) * 4;

            // @TODO: Combine pixels instead of clobbering to get a smoother smear effect?
            // @TODO: *MAYBE* the vertical smear should be more than one row downwards? This might not matter though because it basically works as-is.
            imgData.processedData[currIndex] = imgData.originalData[prevIndex];
            imgData.processedData[currIndex + 1] = imgData.originalData[prevIndex + 1];
            imgData.processedData[currIndex + 2] = imgData.originalData[prevIndex + 2];
            imgData.processedData[currIndex + 3] = 255; // Alpha
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

    images.push_back(imgData);  // Store image data
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
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);

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

        // Create a bitmap in memory which we will use to render the image data
        if(img.hOriginalBitmap == NULL) {
            img.hdcMem = CreateCompatibleDC(hdc);
            img.hOriginalBitmap = BitsToThumbnailBitmap(img.hdcMem, img.width, img.height, img.originalData);
        }

        // Render original thumbnail
        StretchBlt(
            hdc,                    // hdcDest
            xPos,                   // xDest
            yPos,                   // yDest
            thumbnailSize,          // wDest
            thumbnailSize,          // hDest
            img.hdcMem,                 // hdcSrc
            0,                      // xSrc
            0,                      // ySrc
            THUMBNAIL_BASE_SIZE,    // wSrc
            THUMBNAIL_BASE_SIZE,    // hSrc
            SRCCOPY
        );


        /*BitBlt(
            hdc,                    // hdcDest
            xPos,                   // xDest
            yPos,                   // yDest
            thumbnailSize,          // wDest
            thumbnailSize,          // hDest
            img.hdcMem,             // hdcSrc
            0,0, SRCCOPY);
        */
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));  // Red color for edges
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

        MoveToEx(hdc, xPos, yPos, NULL); // Starting point
        LineTo(hdc, xPos + thumbnailSize, yPos + thumbnailSize); // Vertical line

        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);

/*        SelectObject(img.hdcMem, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
  */
        /*
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));  // Red color for edges
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

        for (int col = 0; col < img.detectedColumns.size(); ++col) {
            int tx = img.detectedColumns[col];
            MoveToEx(hdc, thumbnailSize / 500 * (x + thumbnailSpacing + tx), y, NULL); // Starting point
            LineTo(hdc, thumbnailSize / 500 * (x + thumbnailSpacing + tx), y + thumbnailSize); // Vertical line
        }
        */

        /*
        hBitmap = CreateDIBSection(hdc, thumbnailSize, thumbnailSize, &pBits);
        hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

        for (int tx = 1; tx < thumbnailSize - 1; ++tx) {
            int prevX = (tx - 1) * img.width / thumbnailSize;
            int currX = tx * img.width / thumbnailSize;
            int isEdge = 0;

            // Check for edge in the thumbnail
            for (int ty = 0; ty < thumbnailSize; ++ty) {
                int sy = ty * img.height / thumbnailSize;
                int sIndexCurr = (sy * img.width + currX) * 4;
                int sIndexPrev = (sy * img.width + prevX) * 4;

                // Simple edge detection by checking grayscale difference
                BYTE grayCurr = (img.processedData[sIndexCurr] + img.processedData[sIndexCurr + 1] + img.processedData[sIndexCurr + 2]) / 3;
                BYTE grayPrev = (img.processedData[sIndexPrev] + img.processedData[sIndexPrev + 1] + img.processedData[sIndexPrev + 2]) / 3;

                if (abs(grayCurr - grayPrev) > 20) { // Threshold for detecting edges
                    isEdge++;
                    //break;
                }
            }

            if (isEdge == 1) {
                // Draw a thin vertical line in bright color (red)
                MoveToEx(hdc, x + thumbnailSize + thumbnailSpacing + tx, y, NULL); // Starting point
                LineTo(hdc, x + thumbnailSize + thumbnailSpacing + tx, y + thumbnailSize); // Vertical line
            }
        }
        *

        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);

        SelectObject(hdcMem, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        */

        xPos += thumbnailSize + thumbnailSpacing;
        if (xPos + thumbnailSize > clientRect.right) {
            xPos = thumbnailSpacing;
            yPos += (thumbnailSize + thumbnailSpacing) * 2;
        }
    }

    // Update scrollbar
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE;
    si.nMin = 0;
    si.nMax = yPos;
    si.nPage = clientRect.bottom;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}
