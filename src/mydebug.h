#ifndef _MYDEBUG_H
#define _MYDEBUG_H
#include <stdio.h>
#include <string.h>

// How to visualize bytes/words
class BytesToPixelIntf {
public:
	virtual int NumBytesPerPixel() = 0;
	virtual void BytesToPixel(unsigned char* byte_ptr, unsigned char* pixel_ptr) = 0;
	virtual unsigned int Format() = 0;
	virtual ~BytesToPixelIntf() {}
	virtual int NumPixelDataChannels() = 0;
};

// Visualizer state
struct ViewWindow {
	unsigned address, step;
	ViewWindow() : address(0), step(2) { }
};

struct MemView {
	MemView(int _disp_w, int _disp_h);

	void LoadDummy() {
		unsigned char ch = 0;
		for (int i=0; i<bytes_w * bytes_h; i++) {
			if (!UpdateByte(i, ch)) break;
			ch = (ch+1) % 256;
		}
		snprintf(info, sizeof(info), "Dummy data, %d bytes/pixel, showing %.2f KiB",
				bytes2pixel->NumBytesPerPixel(),
				bytes_w * bytes_h / 1024.0f);
	}

  void ConvertToPixels() {
  	const int nc = bytes2pixel->NumPixelDataChannels();
  	const int bp = bytes2pixel->NumBytesPerPixel();
  	unsigned char *byte_ptr = bytes;
  	int px = 0, py = 0;
  	for (; byte_ptr < bytes + bytes_w * bytes_h;
  	       byte_ptr += bp) {
  		unsigned char *pixel_ptr = &pixels[nc * ((disp_h - py) * disp_w + px)];
  		bytes2pixel->BytesToPixel(byte_ptr, pixel_ptr);
  		px ++;
  		if (px >= disp_w) { px = 0; py ++; }
  	}
  }

	void Render();

	// Start writing bytes into the visualization area from DosBox
	void StartUpdateBytes() { }

	// return true if within bounds, false if out of bounds
	bool UpdateByte(int offset, unsigned char ch);

	// End of the write procedure above
	void EndUpdateBytes() { ConvertToPixels(); }

	void SetInfo(const char* x) { snprintf(info, sizeof(info), "%s", x); }

	void SetInfoAddressChangedManually() {
		snprintf(info, sizeof(info), "%08X - %08X Step=%d (%.2f KiB)",
			view_window.address,
			view_window.address + bytes_w * bytes_h * view_window.step,
			view_window.step,
			bytes_w * bytes_h * (view_window.step) / 1024.0f);
	}

	void Pan(int delta); // Pan how many bytes
	void PanLines(int line_delta);

	void Zoom(int zoom_factor);

	// Display properties
	unsigned char* bytes, *pixels;
	int disp_w, disp_h, bytes_w, bytes_h; // 1 pixel = 2 bytes
	int top, left; // top-left x and y
	char info[200];
	BytesToPixelIntf* bytes2pixel;

	// Data properties
	ViewWindow view_window;
};


// For usage from DosBox
void StartMyDebugThread(int argc, char** argv); // Launches visualizer window
void MyDebugStartUpdatingBytes();               // Start update bytes for view window
bool MyDebugUpdateByte(int, unsigned char);     // Updates one byte in the visuzliaer's window
void MyDebugEndUpdateBytes();        // Ends update for the visualizer's window
void MyDebugSetInfo(const char* x);
ViewWindow* MyDebugViewWindow();
bool MyDebugShouldUpdate();

#endif
