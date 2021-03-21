#ifndef _MYDEBUG_H
#define _MYDEBUG_H
#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>

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
	static unsigned max_step, min_step;
	ViewWindow() : address(0), step(2) { }
};

struct MyView {
	virtual void Render() = 0;
	virtual ~MyView() { }
};

struct PerRowHistogram;
struct MemView : public MyView {
	MemView(int _disp_w, int _disp_h);

	void LoadDummy();

	void LoadBufferFile(const char* name);

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

	void Render() override;

	// Start writing bytes into the visualization area from DosBox
	void StartUpdateBytes() { }

	// return true if within bounds, false if out of bounds
	bool UpdateByte(int offset, unsigned char ch);

	// End of the write procedure above
	void EndUpdateBytes() { ConvertToPixels(); }

	void SetInfo(const char* x) { snprintf(info, sizeof(info), "%s", x); }

	void SetInfoAddressChangedManually() {
		char buf[160];
		snprintf(buf, sizeof(buf), "%08X - %08X Step=%d (%.2f KiB)",
			view_window.address,
			view_window.address + bytes_w * bytes_h * view_window.step,
			view_window.step,
			bytes_w * bytes_h * (view_window.step) / 1024.0f);
		if (IsBufferAvailable()) snprintf(info, sizeof(info), "%s (Showing data_buffer)", buf);
		else snprintf(info, sizeof(info), "%s", buf);
	}

	void OnMouseHover(int mouse_x, int mouse_y);
	void Pan(int delta); // Pan how many bytes
	void PanLines(int line_delta);
	void Zoom(int zoom_factor);
	void SetAddress(int addr);
	unsigned GetAddressByPosition(int left, int top);

	// Display properties
	unsigned char* bytes, *pixels;
	int disp_w, disp_h, bytes_w, bytes_h; // 1 pixel = 2 bytes
	int histogram_w;
	int top, left; // top-left x and y
	char info[200];
	BytesToPixelIntf* bytes2pixel;

	// Data properties
	ViewWindow view_window;
	std::vector<unsigned char> data_buffer;
	bool IsBufferAvailable() { return !data_buffer.empty(); }
	void UpdateViewportForBuffer();
	unsigned GetMaxLineStride() { return bytes2pixel->NumBytesPerPixel() * ViewWindow::max_step * disp_w; }
	unsigned GetMinLineStride() { return bytes2pixel->NumBytesPerPixel() * ViewWindow::min_step * disp_w; }
	unsigned GetLineStride()    { return bytes2pixel->NumBytesPerPixel() * view_window.step * disp_w; }

	PerRowHistogram* histogram_write, *histogram_read;
	void IncrementWrite(unsigned phys_addr, int size);
	void IncrementRead(unsigned phys_addr, int size);
	void do_incrementHistogram(PerRowHistogram* hist, unsigned phys_addr, int size);
	void ClearHistograms();
};

// TODO Next step:
// Get the call stack for writes
struct PerRowHistogram {
	static int bucket_limit;
	std::string title;
	float color[3];
	void SetColor(const float r, const float g, const float b) { color[0] = r; color[1] = g; color[2] = b; }
	PerRowHistogram(MemView* _p, const std::string& _title) {
		parent = _p; title = _title;
		buckets.resize(bucket_limit);
		total_count = 0;
	}
	MemView* parent;
	std::vector<unsigned> buckets;
	unsigned total_count;
	void Clear() { for (int i=0; i<buckets.size(); i++) buckets[i] = 0; total_count = 0; }
	void IncrementBucket(int idx) {
		total_count ++;
		if (idx >= 0 && idx < int(buckets.size())) buckets[idx] ++;
	}
	void Render(int left, int top, int w, int h, int line_idx, int buckets_per_line, unsigned max_bin_value);
	unsigned GetBucketValue(int bucket_idx) {
		if (bucket_idx >= 0 && bucket_idx < int(buckets.size())) return buckets[bucket_idx];
		else return 0;
	}
	void LoadDummy();
};

struct TextureView : public MyView {
	std::vector<unsigned char> bytes;
	std::vector<unsigned char> pixels; // Currently, assume pixels == bytes
	BytesToPixelIntf* bytes2pixel;
	std::string title;
	int disp_w, disp_h;
	int top, left;
	TextureView(int w, int h, int size);
	void Render() override;
	void UpdateByte(int offset, unsigned char b);
	void LoadDummy();
	void EndUpdateBytes();
	void ConvertToPixels();
	void SetInfo(const char* x) { title = std::string(x); }
};

struct LogView : public MyView {
	int idx, top, left;
	std::vector<std::string> entries;
	LogView(int capacity);
	void Render() override;
	void AppendEntry(const std::string& line);
	void Clear();
};

// For usage from DosBox
void StartMyDebugThread(int argc, char** argv); // Launches visualizer window
void MyDebugStartUpdatingBytes();               // Start update bytes for view window
bool MyDebugUpdateByte(int, unsigned char);     // Updates one byte in the visuzliaer's window
void MyDebugEndUpdateBytes();        // Ends update for the visualizer's window
void MyDebugSetInfo(const char* x);
ViewWindow* MyDebugViewWindow();
bool MyDebugShouldUpdate();
void MyDebugOnWriteByte(unsigned phys_addr, int size);
void MyDebugOnReadByte(unsigned phys_addr, int size);

void MyDebugTextureViewStartUpdatingBytes();
void MyDebugTextureViewUpdateByte(int, unsigned char); // Updates one byte in the texture viewer
void MyDebugTextureViewEndUpdatingBytes();
void MyDebugTextureViewSetInfo(const char* x);

void MyDebugLogViewAppendEntry(const std::string& line);
void MyDebugOnInstructionEntered(unsigned cs, unsigned seg_cs, unsigned ip);

struct PointCloudView : public MyView {
	int x, y, w, h;
	void Render() override;
};

#endif
