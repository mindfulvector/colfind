# Welcome to the `Image Processor Application`
## also known as `Simple Finder of Scanned-image Text Columns`
### additionally known as `IPA-SFSiTC`

![image](https://github.com/user-attachments/assets/d926db63-8145-463d-849c-9f317cc7f3f3)


This application accepts drag and dropped images (.bmp files only for now), then processes the images to find vertical columns of text.

It provides a simple interface to load images, apply the processing effects, visualize the results, and automatically save XML files.

It can also serve as a Win32 API example program, using only the built-in Windows APIs -- as well as a basic example for manipulating
bitmap data using pure C++98 code.

## Possibly useful example code sections

- **Thumbnail Rendering**: Displays original and processed thumbnails side-by-side.
- **Image Smearing**: Applies a vertical smear effect to the images.
- **Column Detection**: Detects significant vertical columns in images and overlays these detections on the thumbnails.
- **Dynamic Layout**: Thumbnails wrap to the next line if there is insufficient space horizontally.
- **Filename Display**: Displays the filename below each original thumbnail.
- **Scrollable Interface**: If thumbnails exceed the visible area, vertical scrolling is supported.
- **Drag-and-Drop Support**: You can drag image files into the window to process and display them.

## How It Works

1. **Loading Images**: Images are loaded via a file open dialog or drag-and-drop.
2. **Processing**: 
    - The image undergoes vertical smearing where each pixel is replaced by the pixel above it.
    - Vertical columns in the image are detected using edge detection, and their positions are saved.
3. **Rendering**: 
    - Both the original and processed images are rendered as thumbnails.
    - Detected columns are drawn as vertical green lines over the original thumbnails.
    - Filenames of the images are displayed below the original thumbnails.

### User Interface

- **Mouse Wheel Support**: Use the mouse wheel to zoom in or out of the thumbnails.
- **Thumbnails**: Thumbnails are automatically resized and repositioned based on the window size and user interactions.

## Installation and Compilation

Prerequisites:
* [OpenWatcom 2.0](https://github.com/open-watcom/open-watcom-v2/releases)
* Windows (any version supported by OpenWatcom 2.0 should work)

To compile and run this program:

1. Clone the repository:
   ```
   git clone git@github.com:mindfulvector/colfind.git
   ```
2. Open `colfind.wpj` using [OpenWatcom 2.0](https://github.com/open-watcom/open-watcom-v2/releases)
3. Press `F4` or select `Targets` > `Make` from the menu bar
4. To run, press `Ctrl+R` or select `Targets` > `Run` from the menu bar

## License
This code is licensed under the BSD 3-clause license, according to the `LICENSE` file.
