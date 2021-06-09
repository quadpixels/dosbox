#include <mydebug.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <GL/glew.h>
#include <GL/glut.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <assert.h>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <bitset>

#ifndef MEM_VIZ_STANDALONE
#include "cpu.h"
extern void PauseDOSBox(bool pressed);
extern void DEBUG_Enable(bool pressed);
#endif

// Build standalone version:
// g++-8 mydebug.cpp -lGL -lGLEW -lglut -I. -DMEM_VIZ_STANDALONE -lpthread -lX11 -lGLU

GLEW_FUN_EXPORT PFNGLWINDOWPOS2IPROC glWindowPos2i; // Keep Eclipse CDT happy

// Global Variables

PointCloudView* g_pointcloudview;
LogView* g_logview;
bool g_log_writes = false;
bool g_gun_debug = true;
bool g_gun_reveal_minimap = true;
bool g_is_shift = false;
GLfloat g_projection_matrix[16];
void PrintMatrix(const char* title, GLfloat* x) {
	printf("[Print matrix] %s\n", title);
	printf("%2.3f %2.3f %2.3f %2.3f\n", x[0], x[4], x[8], x[12]);
	printf("%2.3f %2.3f %2.3f %2.3f\n", x[1], x[5], x[9], x[13]);
	printf("%2.3f %2.3f %2.3f %2.3f\n", x[2], x[6], x[10],x[14]);
	printf("%2.3f %2.3f %2.3f %2.3f\n", x[3], x[7], x[11],x[15]);
}

char g_message[200];

class BytesToRGB : public BytesToPixelIntf {
public:
	int NumBytesPerPixel() override { return 3; }
	int NumPixelDataChannels() override { return 3; }
	void BytesToPixel(unsigned char* byte_ptr, unsigned char* pixel_ptr) override {
		for (int i=0; i<3; i++) pixel_ptr[i] = byte_ptr[i];
	}
	unsigned int Format() override { return GL_RGB; }
};

class BytesToRG : public BytesToPixelIntf {
public:
	int NumBytesPerPixel() override { return 2; }
	int NumPixelDataChannels() override { return 3; }
	void BytesToPixel(unsigned char* byte_ptr, unsigned char* pixel_ptr) override {
		for (int i=0; i<2; i++) pixel_ptr[i] = byte_ptr[i];
	}
	unsigned int Format() override { return GL_RGB; }
};

class Bytes256ColorToRGB : public BytesToPixelIntf {
public:
	int NumBytesPerPixel() override { return 1; }
	int NumPixelDataChannels() override { return 3; }
	void BytesToPixel(unsigned char* byte_ptr, unsigned char* pixel_ptr) override {
		unsigned char c = *byte_ptr;
		unsigned char blue = (unsigned char)(255 * (c & 3) / 4);
		unsigned char green= (unsigned char)(255 * ((c >> 2) & 7) / 8);
		unsigned char red  = (unsigned char)(255 * ((c >> 5) & 7) / 8);
		pixel_ptr[0] = c;//blue;
		pixel_ptr[1] = c;//green;
		pixel_ptr[2] = c;//red;
	}
	unsigned int Format() override { return GL_RGB; }
};

void render();
void update();
void keyboard(unsigned char, int, int);
void keyboardUp(unsigned char, int, int);
void keyboard2(int, int, int);
void keyboard2Up(int, int, int);
void motion(int x, int y);
void motion_passive(int x, int y);
void mouse(int, int, int, int);
const int WIN_W = 1080, WIN_H = 720;

float DeltaMillis(const struct timeval& from, const struct timeval& to) {
	return (to.tv_sec - from.tv_sec) * 1000.0f +
			   (to.tv_usec- from.tv_usec)/ 1000.0f;
}

struct MouseWheelAccum {
	static float diff; // 50 Milliseconds poll interval
	struct timeval last_timestamp;
	int lines;
	bool ShouldPop() {
		struct timeval curr;
		gettimeofday(&curr, nullptr);
		return (DeltaMillis(last_timestamp, curr) > diff);
	}
	int PopLines() {
		if (ShouldPop()) {
			gettimeofday(&last_timestamp, nullptr);
			int ret = lines; lines = 0;
			return ret;
		} else return 0;
	}
	void PushLines(int x) { lines += x; }
};
float MouseWheelAccum::diff = 50.0f;
MouseWheelAccum g_mousewheel_accum;
std::bitset<11> g_flags;
unsigned ViewWindow::max_step = 64;
unsigned ViewWindow::min_step = 1;

MemView::MemView(int _disp_w, int _disp_h) {
	bytes2pixel = new BytesToRG();

	disp_w = _disp_w; disp_h = _disp_h;
	bytes_w = disp_w * bytes2pixel->NumBytesPerPixel(); bytes_h = disp_h;

	bytes = new unsigned char[bytes_w * bytes_h];
	pixels = new unsigned char[disp_w * disp_h * bytes2pixel->NumPixelDataChannels()];
	memset(bytes, 0x00, bytes_w * bytes_h);
	memset(pixels,0x00, disp_w * disp_h);

	top = 24; left = 4;
	histogram_w = 32;

	histogram_write = new PerRowHistogram(this, "Write");
	histogram_read  = new PerRowHistogram(this, "Read");
	histogram_read->SetColor(0.3, 0.8, 0.3);
	histogram_write->SetColor(0.8, 0.3, 0.3);

	ConvertToPixels();
}

bool MemView::UpdateByte(int offset, unsigned char ch) {
	bool ret = true;
	if (offset >= 0 && offset < bytes_w * bytes_h) {
		bytes[offset] = ch;
	}
	else { ret = false; }
	return ret;
}

void MemView::Render() {
	if (!is_visible) return;
	glWindowPos2i(left, WIN_H - top - disp_h);
	glDrawPixels(disp_w, disp_h, bytes2pixel->Format(), GL_UNSIGNED_BYTE, pixels);
	glWindowPos2i(left, WIN_H - top + 4);
	glColor3f(1, 1, 1);
	for (char* ch = info; *ch != 0x00; ch++) {
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *ch);
	}
	glColor3f(1, 1, 1);
	glBegin(GL_LINE_LOOP);
	const int PAD = 1;
	const int x0 = left-PAD, x1 = left+disp_w+PAD, y0 = WIN_H - (top-PAD), y1 = WIN_H - (top+disp_h+PAD);
	glVertex2i(x0, y0);
	glVertex2i(x0, y1);
	glVertex2i(x1, y1);
	glVertex2i(x1, y0);
	glEnd();

	const int MARGIN = 2;
	// Maybe this needs a fix; PerRowHistogram and MemView classes may be a bit too coupled
	const int bucket_idx = view_window.address / GetMinLineStride();
	const int bucket_per_line = GetLineStride() / GetMinLineStride();
	histogram_write->Render(left + disp_w + MARGIN,     top, histogram_w, disp_h, bucket_idx, bucket_per_line, 0);
	histogram_read ->Render(left + disp_w + 2 * MARGIN + histogram_w, top, histogram_w, disp_h, bucket_idx, bucket_per_line, 0);
}

void MemView::SetAddress(int addr) {
	view_window.address = addr;
	if (IsBufferAvailable()) UpdateViewportForBuffer();
	SetInfoAddressChangedManually();
}

void MemView::Pan(int delta) {
	const unsigned stride = GetLineStride();
	unsigned int* a = &(view_window.address);
	const unsigned ub = (IsBufferAvailable() ? data_buffer.size() : 0xFFFFFFFFU);
	long x = long(*a) + delta;
	if (x < 0) x = 0;
	else if (x > ub) x = ub;

	// Force align to line for consistency of the histogram? Actually not needed
	if (false) {
		*a = (x / stride) * stride;
	} else {
		*a = x;
	}

	if (IsBufferAvailable()) UpdateViewportForBuffer();
	SetInfoAddressChangedManually();
}

void MemView::PanLines(int line_delta) {
	const int bp = bytes2pixel->NumBytesPerPixel();
	const int s  = view_window.step;
	Pan(line_delta * disp_w * bp * s);
}

void MemView::Zoom(int zoom_factor) {
	unsigned int* s = &(view_window.step);
	if (zoom_factor == 1) // Zoom in
	{ if (*s > 1) (*s = *s / 2); }
	else if (*s < 64) {
		*s = *s * 2;
	}
	if (IsBufferAvailable()) UpdateViewportForBuffer();
	SetInfoAddressChangedManually();
}

void MemView::UpdateViewportForBuffer() {
	for (unsigned addr = view_window.address, offset = 0;
			offset < bytes_w * bytes_h; offset++, addr += view_window.step) {
		if (addr >= 0 && addr < data_buffer.size()) {
			UpdateByte(offset, data_buffer[addr]);
		} else UpdateByte(offset, 0x00);
	}
	EndUpdateBytes();
}

void MemView::LoadBufferFile(const char* name) {
	FILE* f = fopen(name, "rb");
	if (0 != fseek(f, 0, SEEK_END)) abort();
	long len = ftell(f);
	rewind(f);
	data_buffer.resize(len);
	assert(len == fread(data_buffer.data(), 1, len, f));
	fclose(f);
	printf("Loaded buffer from file: %s, size: %ld\n", name, len);
	UpdateViewportForBuffer();
	SetInfoAddressChangedManually();
}

void MemView::OnMouseHover(int mouse_x, int mouse_y) {
	const int mouse_top = mouse_y;
	if (mouse_top >= top  && mouse_top <= top + disp_h &&
			mouse_x   >= left && mouse_x   <= left + disp_w) {
		const int x = mouse_x - left, y = mouse_top - top;
		unsigned addr = GetAddressByPosition(x, y);
		snprintf(g_message, sizeof(g_message), "Address: %08X", addr);
	} else {
		snprintf(g_message, sizeof(g_message), "X: %d %d %d, Y: %d %d %d",
				left, mouse_x, left + disp_w, top, mouse_top, top + disp_h);
	}
}

unsigned MemView::GetAddressByPosition(int left, int top) {
	return view_window.address +
			   view_window.step * bytes2pixel->NumBytesPerPixel() * (left + top * disp_w);
}

void MemView::LoadDummy() {
	unsigned char ch = 0;
	for (int i=0; i<bytes_w * bytes_h; i++) {
		if (!UpdateByte(i, ch)) break;
		ch = (ch+1) % 256;
	}
	snprintf(info, sizeof(info), "Dummy data, %d bytes/pixel, showing %.2f KiB",
			bytes2pixel->NumBytesPerPixel(),
			bytes_w * bytes_h / 1024.0f);

	histogram_read->LoadDummy();
	histogram_write->LoadDummy();
}

void MemView::IncrementWrite(unsigned phys_addr, int size) {
	do_incrementHistogram(histogram_write, phys_addr, size);
}

void MemView::IncrementRead(unsigned phys_addr, int size) {
	do_incrementHistogram(histogram_read, phys_addr, size);
}

void MemView::do_incrementHistogram(PerRowHistogram* hist, unsigned phys_addr, int size) {
	const int bucket_idx = phys_addr / GetMinLineStride();
	for (int i=0; i<size; i++) hist->IncrementBucket(bucket_idx);
}

void MemView::ClearHistograms() {
	histogram_write->Clear();
	histogram_read->Clear();
}

MemView* g_memview;

inline int MyLog2(unsigned x) {
	int ret = 0;
	while (x > 0) { ret ++; x >>= 1; }
	return ret;
}

// bucket_idx: Bucket ID (address / line_stride
// max_bin_value: max value to normalize against; if set to 0, will use log(2)
void PerRowHistogram::Render(int left, int top, int w, int h, int bucket_idx, int buckets_per_line, unsigned max_bin_value) {
	if (!is_visible) return;
	const int TEXT_H = 11;
	glWindowPos2i(left, WIN_H - top + 4 + TEXT_H);
	for (char ch : title) { glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, ch); }

	std::string s;
	if (total_count < 1024) s = std::to_string(total_count);
	else if (total_count < 1048576) s = std::to_string(total_count / 1024) + "K";
	else s = std::to_string(total_count / 1024 / 1024) + "M";
	glWindowPos2i(left, WIN_H - top + 4);
	for (char ch : s) { glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, ch); }

	const int PAD = 1;
	const int x0 = left - PAD, x1 = left + w + PAD, y0 = WIN_H - (top - PAD), y1 = WIN_H - (top + h + PAD);
	glBegin(GL_LINE_LOOP);
	glColor3f(1, 1, 1);
	glVertex2i(x0, y0); glVertex2i(x0, y1); glVertex2i(x1, y1); glVertex2i(x1, y0);
	glEnd();

	glBegin(GL_LINES);
	const int XOFFSET = -1;
	for (int y=0; y<h; y++) {
		int line_sum = 0;
		for (int i=0; i<buckets_per_line; i++) {
			line_sum += GetBucketValue(bucket_idx + i);
		}
		bucket_idx += buckets_per_line;

		int dw = 0;
		bool overflowed = false;
		if (line_sum > 0) {

			if (max_bin_value == 0) dw = MyLog2(line_sum);
			else dw = int(line_sum * w / max_bin_value);

			if (dw > w) {
				dw = w; overflowed = true;
			}

			const int dy = WIN_H - (top + y);
			glColor3f(color[0], color[1], color[2]);
			glVertex2i(left + XOFFSET, dy);
			glVertex2i(left + XOFFSET + dw, dy);

			if (overflowed) {
				glColor3f(1, 0, 0);
				glVertex2i(left + XOFFSET + w, dy);
				glVertex2i(left + XOFFSET + w+1, dy);
				glColor3f(color[0], color[1], color[2]);
			}
		}
	}
	glColor3f(1, 1, 1); // Restore color from within glBegin ... glEnd
	glEnd();
}

void PerRowHistogram::LoadDummy() {
	for (int i=0; i<int(buckets.size()); i++) {
		buckets[i] = rand() % 32;
	}
}

int PerRowHistogram::bucket_limit = 1024 * 1024; // 1 M rows

// TextureView
TextureView::TextureView(int w, int h, int size) {
	bytes.resize(size);
	disp_w = w; disp_h = h;
	left = 360; top = 24;
	title = "Texture View";
	bytes2pixel = new Bytes256ColorToRGB();
	pixels.resize(bytes.size() * bytes2pixel->NumPixelDataChannels() / bytes2pixel->NumBytesPerPixel());
}

void TextureView::Render() {
	if (!is_visible) return;
	const int PAD = 1;
	const int x0 = left - PAD, x1 = left + disp_w + PAD, y0 = WIN_H - (top - PAD), y1 = WIN_H - (top + disp_h + PAD);
	glBegin(GL_LINE_LOOP);
	glColor3f(1, 1, 1);
	glVertex2i(x0, y0); glVertex2i(x0, y1); glVertex2i(x1, y1); glVertex2i(x1, y0);
	glEnd();

	glWindowPos2i(left, WIN_H - top + 4);
	for (char ch : title) { glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, ch); }

	glWindowPos2i(left, WIN_H - top - disp_h);
	glDrawPixels(disp_w, disp_h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
}

void TextureView::LoadDummy() {
	for (int i=0; i<int(bytes.size()); i++) { bytes[i] = i % 0xFF; }
	pixels = bytes;
}

void TextureView::UpdateByte(int offset, unsigned char b) {
	if (offset >= 0 && offset < int(bytes.size())) bytes[offset] = b;
}

void TextureView::ConvertToPixels() {
	const int step_b = bytes2pixel->NumBytesPerPixel();
	const int step_p = bytes2pixel->NumPixelDataChannels();
	int idx_b = 0, idx_p = 0;
	for (;
			 idx_b < int(bytes.size()) && idx_p < int(pixels.size());
			 idx_b += step_b, idx_p += step_p) {
		bytes2pixel->BytesToPixel(bytes.data() + idx_b, pixels.data() + idx_p);
	}
}

void TextureView::EndUpdateBytes() {
	ConvertToPixels();
}

TextureView* g_texture_view;

// Helper
struct timeval g_last_update;
void MyDebugStartUpdatingBytes() { g_memview->StartUpdateBytes(); }
bool MyDebugUpdateByte(int offset, unsigned char ch) { return g_memview->UpdateByte(offset, ch); }
void MyDebugEndUpdateBytes() {
	g_memview->EndUpdateBytes();
	gettimeofday(&g_last_update, nullptr);
}
void MyDebugSetInfo(const char* info) {
	g_memview->SetInfo(info);
}
float DiffInUsec(const struct timeval& lb, const struct timeval& ub) {
	return 1000000.0f * (ub.tv_sec - lb.tv_sec) + (ub.tv_usec - lb.tv_usec);
}
const int UPDATE_FPS = 30;
bool MyDebugShouldUpdate() {
	struct timeval curr;
	gettimeofday(&curr, nullptr);
	float diff = DiffInUsec(g_last_update, curr);
	if (diff >= 1000000.0f / UPDATE_FPS) { return true; }
	else return false;
}
void MyDebugOnReadByte(unsigned phys_addr, int size) {
	g_memview->IncrementRead(phys_addr, size);
}
void MyDebugOnWriteByte(unsigned phys_addr, int size) {
	g_memview->IncrementWrite(phys_addr, size);
	if (g_log_writes) {
		std::stringstream x;
		x << "Write " << std::to_string(size) << "B @ " << std::hex << std::setw(8) << std::setfill('0') << phys_addr;
		g_logview->AppendEntry(x.str());
	}
}

void MyDebugTextureViewStartUpdatingBytes() {
	// Do nothing for now
}

void MyDebugTextureViewUpdateByte(int offset, unsigned char b) {
	g_texture_view->UpdateByte(offset, b);
}

void MyDebugTextureViewSetInfo(const char* x) {
	g_texture_view->SetInfo(x);
}

void MyDebugTextureViewEndUpdatingBytes() {
	g_texture_view->EndUpdateBytes();
}

pthread_t g_thread;
int g_argc; char** g_argv;
void* MyDebugInit(void* x);

void StartMyDebugThread(int argc, char** argv) {
	g_argc = argc; g_argv = argv;
	g_memview = new MemView(256, 512);
	g_memview->SetInfoAddressChangedManually();
	g_texture_view = new TextureView(320, 240, 320*240*3);
	// Load file
	// Set data source
	if (argc > 1) {
		g_memview->LoadBufferFile(argv[1]);
	}

	g_logview = new LogView(50);
	g_logview->AppendEntry("LogView created.");
	g_pointcloudview = new PointCloudView();
	g_pointcloudview->x = 360;
	g_pointcloudview->y = 360;
	g_pointcloudview->w = 300;
	g_pointcloudview->h = 300;
	g_pointcloudview->SaveXYWH();

	if (argc > 1) {
		g_pointcloudview->ReadFromFile(argv[1]);
	}

	pthread_create(&g_thread, NULL, MyDebugInit, 0);
}

void* MyDebugInit(void* x) {
	printf("[MyDebugInit] Hey!\n");
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutInitWindowSize(WIN_W, WIN_H);
	glutInit(&g_argc, g_argv);
	glutCreateWindow("Hello World");
	glutDisplayFunc(render);
	glutIdleFunc(update);
	glutKeyboardFunc(keyboard); // This will not work with Dosbox due to multithreading problems
	glutKeyboardUpFunc(keyboardUp);
	glutSpecialFunc(keyboard2);
	glutSpecialUpFunc(keyboard2Up);
	glutMouseFunc(mouse);
	glutMotionFunc(motion);
	glutPassiveMotionFunc(motion_passive);
	XInitThreads();
	glOrtho(0, WIN_W, 0, WIN_H, 0, 1); // Use windowPos coordinate system

	int major_version, minor_version;
	glGetIntegerv(GL_MAJOR_VERSION, &major_version);
	glGetIntegerv(GL_MINOR_VERSION, &minor_version);
	printf("GL_MAJOR_VERSION=%d, GL_MINOR_VERSION=%d\n", major_version, minor_version);
	printf("GL_VERSION=%s\n", glGetString(GL_VERSION));
#ifdef MEM_VIZ_STANDALONE
#endif

	glewInit();
	glutMainLoop();
	return NULL;
}

bool is_pointcloud_maximized = false;
void SwitchMaximizedPointCloudViewMode() {
	if (is_pointcloud_maximized == false) {
		g_logview->is_visible = false;
		g_memview->is_visible = false;
		g_texture_view->is_visible = false;
		g_pointcloudview->Maximize();
		is_pointcloud_maximized = true;
	} else {
		g_logview->is_visible = true;
		g_memview->is_visible = true;
		g_texture_view->is_visible = true;
		g_pointcloudview->LoadXYWH();
		is_pointcloud_maximized = false;
	}

}

ViewWindow* MyDebugViewWindow() { return &(g_memview->view_window); }

void update() {

	// Pop
	const int l = g_mousewheel_accum.PopLines();
	if (l != 0) g_memview->PanLines(l);

	// Move camera
	if (g_pointcloudview) {
		Camera* camera = &(g_pointcloudview->camera);
		const float mult = g_pointcloudview->speed_multiplier;
		const float linspeed = 1 * mult, rotspeed = 0.04;
		if (g_flags.test(0)) { camera->MoveAlongXZ(0, -linspeed); }
		if (g_flags.test(1)) { camera->MoveAlongXZ(0,  linspeed); }
		if (g_flags.test(2)) { camera->MoveAlongLocalAxis(glm::vec3( linspeed, 0, 0)); }
		if (g_flags.test(3)) { camera->MoveAlongLocalAxis(glm::vec3(-linspeed, 0, 0)); }
		if (g_flags.test(4)) { camera->RotateAlongLocalAxis(glm::vec3( 1,  0, 0), rotspeed); }
		if (g_flags.test(5)) { camera->RotateAlongLocalAxis(glm::vec3(-1,  0, 0), rotspeed); }
		if (g_flags.test(6)) { camera->RotateAlongGlobalAxis(glm::vec3( 0,  1, 0), rotspeed); }
		if (g_flags.test(7)) { camera->RotateAlongGlobalAxis(glm::vec3( 0, -1, 0), rotspeed); }
		if (g_flags.test(8)) { camera->MoveAlongGlobalAxis(glm::vec3(0,  linspeed, 0)); }
		if (g_flags.test(9)) { camera->MoveAlongGlobalAxis(glm::vec3(0, -linspeed, 0)); }
	}

	glutPostRedisplay();
}

int g_frame_count = 0;
void render() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
	glDisable(GL_LIGHTING);

	g_memview->Render();
	g_texture_view->Render();
	g_logview->Render();
	g_pointcloudview->Render();

	glWindowPos2i(0, 4);
	glColor3f(1, 1, 1);
	const char* ch = g_message;
	while (*ch != 0) {
		glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *ch);
		ch++;
	}

	glutSwapBuffers();
	g_frame_count ++;
}

void keyboard(unsigned char key, int x, int y) {
	bool is_shift = glutGetModifiers() & GLUT_ACTIVE_SHIFT;

	if (is_pointcloud_maximized) {
		switch (key) {
			case 8: g_pointcloudview->UnfocusFace(); break;
			case '[': g_pointcloudview->CycleToPreviousFace(); break;
			case ']': g_pointcloudview->CycleToNextFace(); break;
			case '-': g_pointcloudview->CycleToPreviousObject(); break; // Zoom out
			case '=': g_pointcloudview->CycleToNextObject(); break; // Zoom in
			default: break;
		}
	} else {
		switch (key) {
			case '[': g_memview->PanLines(-32); break;
			case ']': g_memview->PanLines( 32); break;
			case '-': g_memview->Zoom(-1); break; // Zoom out
			case '=': g_memview->Zoom(1); break; // Zoom in
			default: break;
		}
	}

	switch (key) {
#ifdef MEM_VIZ_STANDALONE
		case 27: exit(0); break;
#endif
		case 'w': g_log_writes = !g_log_writes; break;
		case 's': g_pointcloudview->WriteToFile("POINT_CLOUD"); break;
#ifdef MEM_VIZ_STANDALONE
		case 'r': {
			g_memview->LoadDummy();
			g_texture_view->LoadDummy();
		}
		break;
#else
		case 'r': {
			g_logview->Clear();
			g_memview->ClearHistograms();
			g_pointcloudview->RequestClear();
			break;
		}
		case 'P': { // Gun-specific debug settings.
			DEBUG_Enable(true);
			g_logview->AppendEntry("Setting core type to Simple");
			g_memview->SetAddress(0x8394f0);
			break;
		}
		case 'g': g_gun_debug = !g_gun_debug; break;
#endif
		case 'i': g_flags.set(0); break;
		case 'k': g_flags.set(1); break;
		case 'j': g_flags.set(6); break;
		case 'l': g_flags.set(7); break;
		case 'u': g_flags.set(8); break;
		case 'o': g_flags.set(9); break;

		case 'I': g_flags.set(4); break;
		case 'K': g_flags.set(5); break;
		case 'J': g_flags.set(2); break;
		case 'L': g_flags.set(3); break;

		case 'p': g_pointcloudview->ChangeSpeedMultiplier(2.0f); break;
		case ';': g_pointcloudview->ChangeSpeedMultiplier(0.5f); break;
		case 9:  SwitchMaximizedPointCloudViewMode(); break;
	}
}

void keyboardUp(unsigned char key, int x, int y) {
	switch (key) {
		case 'i': g_flags.reset(0); break;
		case 'k': g_flags.reset(1); break;
		case 'j': g_flags.reset(6); break;
		case 'l': g_flags.reset(7); break;
		case 'I': g_flags.reset(4); break;
		case 'K': g_flags.reset(5); break;
		case 'J': g_flags.reset(2); break;
		case 'L': g_flags.reset(3); break;
		case 'u': g_flags.reset(8); break;
		case 'o': g_flags.reset(9); break;
	}
}

void keyboard2(int key, int x, int y) {
	switch (key) {
		case GLUT_KEY_PAGE_UP: g_memview->Pan(-1024); break;
		case GLUT_KEY_PAGE_DOWN: g_memview->Pan(1024); break;
		case GLUT_KEY_UP: g_memview->PanLines(-1); break;
		case GLUT_KEY_DOWN: g_memview->PanLines(1); break;
		case GLUT_KEY_LEFT: g_memview->Pan(-8); break;
		case GLUT_KEY_RIGHT:g_memview->Pan( 8); break;
	}
}

void keyboard2Up(int key, int x, int y) {
}

// Accumulate
void mouse(int button, int state, int x, int y) {
	snprintf(g_message, sizeof(g_message), "Mouse: (%d,%d,button=%d,state=%d)", x, y, button, state);
	if (button == 3) { // Scroll up
		g_mousewheel_accum.PushLines(-16);
	} else if (button == 4) {
		g_mousewheel_accum.PushLines( 16);
	}
}

void motion(int x, int y) {
	snprintf(g_message, sizeof(g_message), "Motion: (%d,%d)", x, y);
	g_memview->OnMouseHover(x, y);
}

void motion_passive(int x, int y) {
	snprintf(g_message, sizeof(g_message), "Motion-passive: (%d,%d)", x, y);
	g_memview->OnMouseHover(x, y);
}

#ifdef MEM_VIZ_STANDALONE
int main(int argc, char** argv) {
  //MyDebugInit(argc, argv);
	StartMyDebugThread(argc, argv);
	pthread_join(g_thread, NULL);
}
#endif

LogView::LogView(int capacity) {
	entries.resize(capacity);
	idx = 0;
	left = 720; top = 20;
}

void LogView::Render() {
	if (!is_visible) return;
	const int LINE_HEIGHT = 12;
	const int disp_w = 320, disp_h = LINE_HEIGHT * int(entries.size());
	const int PAD = 1;
	const int x0 = left - PAD, x1 = left + disp_w + PAD, y0 = WIN_H - (top - PAD), y1 = WIN_H - (top + disp_h + PAD);
	glBegin(GL_LINE_LOOP);
	glColor3f(1, 1, 1);
	glVertex2i(x0, y0); glVertex2i(x0, y1); glVertex2i(x1, y1); glVertex2i(x1, y0);
	glEnd();

	for (int y=0; y<int(entries.size()); y++) {
		glWindowPos2i(left, WIN_H - top - LINE_HEIGHT * (1+y) + 2);
		const std::string& entry = entries[(idx+y) % int(entries.size())];
		for (char ch : entry) { glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, ch); }
	}
}

void LogView::AppendEntry(const std::string& line) {
	entries[idx] = line;
	idx = (idx + 1) % (int(entries.size()));
}

void LogView::Clear() {
	for (int i=0; i<int(entries.size()); i++) { entries[i].clear(); }
}

// from debug.cpp, on my current system
#ifdef MEM_VIZ_STANDALONE
typedef unsigned short     Bit16u;
typedef unsigned int       Bit32u;
typedef unsigned long long Bit64u;
typedef unsigned int       PhysPt;
#endif

#ifndef MEM_VIZ_STANDALONE
extern Bit32u GetAddress(Bit16u seg, Bit32u offset);
extern Bit32u mem_readd(PhysPt address);
Bit64u MyReadQ(PhysPt address) {
	Bit64u low32 = (Bit64u)(mem_readd(address));
	Bit64u high32 = (Bit64u)(mem_readd(address+4));
	return (high32<<32) | low32;
}
extern unsigned Get32BitRegister(const std::string& name);
#else // Dummy for standalone version
Bit32u GetAddress(Bit16u seg, Bit32u offset) { return 0; }
Bit32u mem_readd(PhysPt address) { return 0; }
Bit32u mem_readb(PhysPt address) { return 0; }
Bit64u MyReadQ(PhysPt address) { return 0; }
unsigned Get32BitRegister(const std::string& name) { return 0; }
#endif

// [ESP + esp_offset]
unsigned GetStackVar(int esp_offset) {
	unsigned addr = Get32BitRegister("esp");
	return mem_readd(GetAddress(0x0168, addr + esp_offset));
}

// [ESI + 8*in_EAX_index]
unsigned GetSourceVar(int in_EAX_index) {
	unsigned addr = Get32BitRegister("esi");
	return mem_readd(GetAddress(0x0168, addr + in_EAX_index * 8));
}

glm::vec3 ReadVec3DoublePrecision(PhysPt address) {
	long x = mem_readd(address     ) | (long(mem_readd(address + 4 )) << 32);
	long y = mem_readd(address + 8 ) | (long(mem_readd(address + 12)) << 32);
	long z = mem_readd(address + 16) | (long(mem_readd(address + 20)) << 32);

	glm::vec3 ret;
	ret.x = *reinterpret_cast<double*>(&x);
	ret.y = *reinterpret_cast<double*>(&y);
	ret.z = *reinterpret_cast<double*>(&z);
	return ret;
}

glm::vec3 ReadVec3SinglePrecision(PhysPt address) {
	int x = mem_readd(address    );
	int y = mem_readd(address + 4);
	int z = mem_readd(address + 8);

	glm::vec3 ret;
	ret.x = *reinterpret_cast<float*>(&x);
	ret.y = *reinterpret_cast<float*>(&y);
	ret.z = *reinterpret_cast<float*>(&z);
	return ret;
}

int g_curr_vertex_idx = 0;

void MyDebugOnInstructionEntered(unsigned cs, unsigned seg_cs, unsigned ip, int* status) {
	// GUN DBG
	char buf[111];
	
	if (seg_cs == 0x0160 && ip == 0x283d17) {
		int dw0 = mem_readd(GetAddress(0x0160, 0x5214a0));
		int dw1 = mem_readd(GetAddress(0x0160, 0x5214a4));
		int dw2 = mem_readd(GetAddress(0x0160, 0x5214a8));
		int dw3 = mem_readd(GetAddress(0x0160, 0x5214ac));
		int dw4 = mem_readd(GetAddress(0x0160, 0x52149c));
		// Suspect: viewport
		// Typical valus: [1,-144,144,-62], single player, 90% view
		//                [1,-160,160,-62], single player, 100% view
		//                [1,-124,124,-48], multiplayer
		sprintf(buf, "%04X:%08X, 0x5214a0:[%d,%d,%d,%d], ustack56=%X", seg_cs, ip,
		  dw0, dw1, dw2, dw3, dw4);
		g_logview->AppendEntry(buf);
	} else if (seg_cs == 0x0160 && ip == 0x283e7e) {
		if (g_gun_reveal_minimap) {

			// Determine if player can see this face
			// if (iStack212 != 0) || 
			// (*(iStack148+0x7c) & 2 == 0 && *(iStack148 + 0x7c) & 1 != 0) then draw
			unsigned ebx = Get32BitRegister("ebx");
			unsigned edx = Get32BitRegister("edx");
			char iStack148 = mem_readb(GetAddress(0x0160, edx + 0x7c));
			bool visited = (ebx != 0 || ((iStack148 & 2) == 0 || (iStack148 & 1) != 0));
			g_pointcloudview->SetVertexVisibility(g_curr_vertex_idx, visited);
			g_curr_vertex_idx ++;

			//g_logview->AppendEntry("Attempting to reveal");
			*status = 1;
		}
	}

	const bool DUMP_METHOD = 1; // 1: Dump everything at the beginning
	                            // 2: Dump everything during the rendering process

	// Polygon capture process
	if (seg_cs == 0x0160 && ip == 0x283cb0) { // Entry point is 283c90, setting ESI done at 283cb0
		g_curr_vertex_idx = 0;
		if (g_pointcloudview->should_clear) {
			g_pointcloudview->should_clear = false;
			g_pointcloudview->Clear();
			g_pointcloudview->should_append = true;

			// SCRAPE

			int dw5 = mem_readd(GetAddress(0x0160, 0x490eac));
			
			// Level 1 descriptor size is 0x18
			int fStack160 = GetStackVar(0x3bc); // ESP + 0x3BC
			int inEax19 = GetSourceVar(0x13);


			sprintf(buf, "[490eac]=%d, fStack160=%X, *in_EAX[0x13]=%d", dw5, fStack160, inEax19);
			g_logview->AppendEntry(buf);

			if (DUMP_METHOD == 1) {
				int tot_num_faces = 0, tot_num_verts = 0;
				for (int i=0; i<dw5; i++) {
					int piStack224 = inEax19 + 24 * i;
					int n1 = mem_readd(GetAddress(0x0168, piStack224));

					sprintf(buf, "[%d] has %d children piStack224=%X\n", i, n1, piStack224);
					g_logview->AppendEntry(buf);

					if (n1 > 0) {
						for (int j=0; j<n1; j++) {

							int tot_num_faces_before = tot_num_faces;

							int addr1 = mem_readd(GetAddress(0x0168, piStack224+4)) + 4*j; // piStack224[1] + iStack216
							int iStack192 = mem_readd(GetAddress(0x0168, addr1));
							int num_faces = mem_readd(GetAddress(0x0168, iStack192 + 0x6a)) & 0xFFFF; // Offset 0x6a (106)

							// This object's metadata
							std::vector<char> om;
							for (int k=0; k<128; k++) { om.push_back(mem_readb(GetAddress(0x0168, k+iStack192))); }
							g_pointcloudview->object_metadata.push_back(om);

							// Faces' metadata
							std::vector<std::vector<char> > fm;

							// The object's transformation
							glm::vec3 obj_pos = ReadVec3DoublePrecision(GetAddress(0x0168, iStack192));
							glm::mat3 obj_orientation;
							obj_orientation[0] = ReadVec3DoublePrecision(GetAddress(0x0168, iStack192 + 24));
							obj_orientation[1] = ReadVec3DoublePrecision(GetAddress(0x0168, iStack192 + 48));
							obj_orientation[2] = ReadVec3DoublePrecision(GetAddress(0x0168, iStack192 + 72));

							// The object's vertex buffer
							int vb = mem_readd(GetAddress(0x0168, iStack192 + 0x64)); // Offset 0x64 (100)

							// Face data
							int face_data = mem_readd(GetAddress(0x0168, iStack192 + 0x6c));

							for (int fidx = 0; fidx < num_faces; fidx++) {
								int pfd = face_data + 0x80 * fidx;
								int num_verts = mem_readb(GetAddress(0x0168, pfd + 0x71));

								// Face's metadata
								std::vector<char> fm_this;
								for (int k=0; k<128; k++) { fm_this.push_back(mem_readb(GetAddress(0x0168, k+pfd))); }
								fm.push_back(fm_this);

								g_pointcloudview->BeginNewPolygon();
								tot_num_faces ++;
								int pindexbuffer = mem_readd(GetAddress(0x0168, pfd + 0x006c));
								for (int vidx = 0; vidx < num_verts; vidx++) {
									int vb_offset = mem_readd(GetAddress(0x0168, pindexbuffer + 2 * vidx)) & 0xFFFF;
									glm::vec3 v_local = ReadVec3SinglePrecision(GetAddress(0x0168, vb + 12 * vb_offset));
									glm::vec3 v_world = obj_pos + obj_orientation * v_local;
									g_pointcloudview->AddGunVertex(v_world);
									tot_num_verts ++;
								}
							}
							g_pointcloudview->object_polygon_ranges.push_back(std::make_pair(tot_num_faces_before, tot_num_faces));
							g_pointcloudview->face_metadata.push_back(fm);
						}
					}
				}
				sprintf(buf, "Total %d objects, %d faces, %d verts\n", dw5, tot_num_faces, tot_num_verts);
							g_logview->AppendEntry(buf);
			}
		}
		else if (g_pointcloudview->should_append == true &&
		         g_pointcloudview->num_verts > 0) {
			g_pointcloudview->should_append = false;
		}
	}
	
	if (DUMP_METHOD == 2) {
		if (seg_cs == 0x0160 && ip == 0x283ec3) {
			g_pointcloudview->BeginNewPolygon();
		} else if (seg_cs == 0x0160 && ip == 0x2842aa) {
			if (g_pointcloudview->should_append) {
				unsigned eax = Get32BitRegister("eax");
				Bit64u elt0 = MyReadQ(GetAddress(0x0168, eax));
				Bit64u elt1 = MyReadQ(GetAddress(0x0168, eax+8));
				Bit64u elt2 = MyReadQ(GetAddress(0x0168, eax+16));
				double d0 = *(reinterpret_cast<double*>(&elt0));
				double d1 = *(reinterpret_cast<double*>(&elt1));
				double d2 = *(reinterpret_cast<double*>(&elt2));
				glm::vec3 v = glm::vec3(float(d0), float(d1), float(d2));
				g_pointcloudview->AddGunVertex(v);
			}
		}
	}
}

void MyPushMatrixes() {
	glMatrixMode(GL_PROJECTION); glPushMatrix();
	glMatrixMode(GL_MODELVIEW);  glPushMatrix();
}

void MyPopMatrixes() {
	glMatrixMode(GL_PROJECTION); glPopMatrix();
	glMatrixMode(GL_MODELVIEW);  glPopMatrix();
}

PointCloudView::PointCloudView() {
	std::vector<glm::vec3> vertices = {
		glm::vec3(-368.0f, -416.0f, -442.0f),
		glm::vec3(-168.0f, -386.0f, -402.0f),
		glm::vec3(-268.0f, -376.0f, -242.0f),
	};
	for (int i=0; i<vertices.size(); i++) {
		AddGunVertex(vertices[i]);
	}
	should_clear = false;
	should_append = true;
	speed_multiplier = 512.0f;
	num_verts = 0;
}

void Camera::Apply() {
	//
	// 1. About data layout
	// M is column-major
	//
	// glm::mat3 is a set of 3 glm::vec3's, each vec3 is a column in the mat3,
	// so the first subscript to glm::mat3 is column index
	// & the second subscript to glm::mat3 is row index
	//
	// So if we write the desired MV matrix in textbook form:
	//
	// M[0]  M[4]  M[8]  M[12]
	// M[1]  M[5]  M[9]  M[13]
	// M[2]  M[6]  M[10] M[14]
	// M[3]  M[7]  M[11] M[15]
	//
	// Which is equivalent to
	//
	// o[0][0]  o[1][0]  o[2][0]  pos.x
	// o[0][1]  o[1][1]  o[2][1]  pos.y
	// o[0][2]  o[1][2]  o[2][2]  pos.z
	// 0        0        0        1
	//
	// 2. About transformation
	//
	// The MV matrix transforms the world. The camera remains at the origin.
	// As such we set the orientation to the inverse, and set the position
	// to the position in camera's local space.
	//
	float M[16];
	const glm::mat3 o = glm::inverse(orientation);
	const glm::vec3 p = o * pos;
	M[0] = o[0].x;
	M[1] = o[0].y;
	M[2] = o[0].z;
	M[3] = 0;
	M[4] = o[1].x;
	M[5] = o[1].y;
	M[6] = o[1].z;
	M[7] = 0;
	M[8] = o[2].x;
	M[9] = o[2].y;
	M[10] = o[2].z;
	M[11] = 0;
	M[12] = p.x;
	M[13] = p.y;
	M[14] = p.z;
	M[15] = 1;

	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(M);
}

void Camera::RotateAlongLocalAxis(const glm::vec3& axis, const float radians) {
	const float c = cosf(radians), s = sinf(radians);
	const float ux = axis.x, uy = axis.y, uz = axis.z;
	glm::mat3 r;
	r[0][0] = c+ux*ux*(1-c);    r[1][0] = ux*uy*(1-c)-uz*s; r[2][0] = ux*uz*(1-c)+uy*s;
	r[0][1] = uy*ux*(1-c)+uz*s; r[1][1] = c + uy*uy*(1-c);  r[2][1] = uy*uz*(1-c)-ux*s;
	r[0][2] = uz*ux*(1-c)-uy*s; r[1][2] = uz*uy*(1-c)+ux*s; r[2][2] = c+uz*uz*(1-c);
	orientation *= r;
}

void Camera::RotateAlongGlobalAxis(const glm::vec3& axis, const float radians) {
	RotateAlongLocalAxis(glm::inverse(orientation) * axis, radians);
}

void Camera::MoveAlongLocalAxis(const glm::vec3& delta_pos) {
	glm::vec3 p = orientation * delta_pos;
	pos += p;
}

void Camera::MoveAlongGlobalAxis(const glm::vec3& delta_pos) {
	glm::vec3 p = delta_pos;
	pos += p;
}

void Camera::MoveAlongXZ(const float dx, const float dz) {
	glm::vec3 z = orientation[2] * (-1.0f);
	z.y = 0;
	if (glm::dot(z, z) < 1e-4) z = glm::vec3(1, 0, 0);
	z = glm::normalize(z);
	glm::vec3 x;
	x.y = 0; x.x = z.z; x.z = -z.x;
	pos += (x*dx + z*dz);
}

void DrawHex(const std::vector<char>& data, int x, int y, int line_width, int char_limit) {
	const int N = std::min(char_limit, int(data.size()));
	int dx = x, dy = y;

/*	glWindowPos2i(x, WIN_H - y + 4);
	glColor3f(1, 1, 1);
	for (char* ch = tmp; *ch != 0x00; ch++) {
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *ch);
	}
*/

	for (int i=0; i<N; i++) {
		if (i % line_width == (line_width - 1)) {
			dx = x; dy -= 15;
		}
		else if (i % line_width == 0) {
			glWindowPos2i(dx, dy);
		}
		char buf[10];
		snprintf(buf, 6, "%02X ", data[i]);
		for (int i=0; i<2; i++) {
			glutBitmapCharacter(GLUT_BITMAP_8_BY_13, buf[i]);
		}
		glutBitmapCharacter(GLUT_BITMAP_8_BY_13, ' ');
	}
}

void PointCloudView::Render() {
	const int PAD = 1;
	const int x0 = x - PAD, x1 = x + w + PAD, y0 = WIN_H - (y - PAD), y1 = WIN_H - (y + h + PAD);
	glBegin(GL_LINE_LOOP);
	glColor3f(1, 1, 1);
	glVertex2i(x0, y0); glVertex2i(x0, y1); glVertex2i(x1, y1); glVertex2i(x1, y0);
	glEnd();

	// Default:
	// [ 1 0 0 0 ]
	// [ 0 1 0 0 ]
	// [ 0 0 1 0 ]
	// [ 0 0 0 1 ]
	if (g_frame_count <= 10) {
		glGetFloatv(GL_PROJECTION_MATRIX, g_projection_matrix);
		//PrintMatrix("projection matrix", g_projection_matrix);
	}

	glMatrixMode(GL_PROJECTION);
	
	glViewport(x, WIN_H-y-h, w, h);
	glLoadIdentity();
	gluPerspective(60, w*1.0f/h, 0.1, 100000);
	
	const float rot_x = g_frame_count * 0.5f, rot_y = g_frame_count * 0.2f;
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	// Translate
	const int N = std::max(1, num_verts);
	glm::vec3 center = sum*(1.0f / N);
	glm::vec3 extent = bb_ub - bb_lb;
	

	camera.Apply();

	// Do not draw vertices
	#if 0
	glPointSize(3);
	glBegin(GL_POINTS);
	
	for (int i=0; i<int(polygons.size() + 1); i++) {
		std::vector<glm::vec3>* p = nullptr;
		if (i == polygons.size()) p = &(curr_polygon);
		else p = &(polygons[i]);
		for (int j=0; j<int(p->size()); j++) {
			glm::vec3 v = GunCoordToOpenglCoord(p->at(j));
			glVertex3f(v.x - (center.x),
								v.y - (center.y), 
								v.z - (center.z));
		}
	}
	glEnd();
	glPointSize(1);
	#endif

	for (int i=0; i <= int(polygons.size()); i++) {
		std::vector<glm::vec3>* p;
		if (i == int(polygons.size())) {
			p = &(curr_polygon);
		} else {
			p = &(polygons[i]);
		}

		bool vert_visited = true;
		if (i < visited.size()) vert_visited = visited[i];

		glBegin(GL_LINE_LOOP);
		for (int j=0; j<int(p->size()); j++) {
			glm::vec3 v = GunCoordToOpenglCoord(p->at(j));
			glm::vec3 cp = camera.pos;
			
			glm::vec3 vp = v - center + cp;
			float dist = glm::dot(vp, vp);
			float intensity = 1;
			const float threshold = 222222;
			if (dist > threshold) {
				intensity = 0.2;
			} else {
				intensity = 1 - (dist / threshold * 0.8f);
			}

			// Set Color
			if (vert_visited) {
				glColor3f(intensity, intensity, intensity); 
			}
			else { glColor3f(intensity, intensity/3, intensity/3); }

			glVertex3f(v.x - (center.x),
								 v.y - (center.y), 
								 v.z - (center.z));
		}
		glEnd();

		glColor3f(1, 1, 1);
	}

	// Draw the focused object
	if (focused_object_idx != -999 && focused_object_idx < object_polygon_ranges.size()) {
		std::pair<int, int> range = object_polygon_ranges[focused_object_idx];
		glDepthFunc(GL_ALWAYS);
		const int fc = range.second - range.first;
		for (int j=0, pidx = range.first; j<=fc; j++, pidx++) {
			std::vector<glm::vec3>* p;
			if (pidx == int(polygons.size()) || j == fc) {
				p = &(polygons[range.first + focused_face_idx]);
			} else {
				p = &(polygons[pidx]);
			}

			glBegin(GL_LINE_LOOP);
			for (int k=0; k<p->size(); k++) {
				glm::vec3 v = GunCoordToOpenglCoord(p->at(k));
				if (j == fc) { glColor3f(1,1,0); }
				else glColor3f(0, 1, 0);
				glVertex3f(v.x - center.x, v.y - center.y, v.z - center.z);
			}
			glColor3f(1, 1, 1);
			glEnd();
		}
		glDepthFunc(GL_LESS);
	}

	glPopMatrix();

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-1, 1, -1, 1, -1, 1);
	glViewport(0, 0, WIN_W, WIN_H);
	char tmp[200];
	
	sprintf(tmp, "%d verts, %d polys, center:(%.2f, %.2f, %.2f)",
		int(num_verts), GetPolyCount(),
		center.x, center.y, center.z
	);

	glWindowPos2i(x, WIN_H - y + 4);
	glColor3f(1, 1, 1);
	for (char* ch = tmp; *ch != 0x00; ch++) {
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *ch);
	}

	sprintf(tmp, "bb:(%.2f,%.2f,%.2f)-(%.2f,%.2f,%.2f)",
		bb_lb.x, bb_lb.y, bb_lb.z,
		bb_ub.x, bb_ub.y, bb_ub.z);
	glWindowPos2i(x, WIN_H - y + 16);
	for (char* ch = tmp; *ch != 0x00; ch++) {
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *ch);
	}

	const glm::vec3& cp = camera.pos;
	sprintf(tmp, "cam: (%.2f, %.2f, %.2f), flags=%s, mult:%.2f",
		cp.x, cp.y, cp.z, g_flags.to_string().c_str(),
		speed_multiplier);
	glWindowPos2i(x, WIN_H - y + 28);
	for (char* ch = tmp; *ch != 0x00; ch++) {
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *ch);
	}

	glWindowPos2i(x, WIN_H - y + 40);

	sprintf(tmp, "Vertex View Total %d objects, %d faces\n",
	int(object_polygon_ranges.size()), int(polygons.size()));

	for (int i=0; i<strlen(tmp); i++) {
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, tmp[i]);
	}

	if (is_pointcloud_maximized && focused_object_idx != -999) {
		if (focused_object_idx < object_metadata.size()) {
			const float deltay = -8;
			glWindowPos2i(x+3, WIN_H-y-2+deltay);
			sprintf(tmp, "Object %d metadata:", focused_object_idx);
			for (int i=0; i<strlen(tmp); i++) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, tmp[i]);
			DrawHex(object_metadata[focused_object_idx], x+3, WIN_H-y-17+deltay, 32, 128);

			if (focused_object_idx < face_metadata.size() &&
					focused_face_idx < face_metadata[focused_object_idx].size()) {
				glWindowPos2i(x+3, WIN_H-y-80+deltay);
				sprintf(tmp, "Face %d metadata:", focused_face_idx);
				for (int i=0; i<strlen(tmp); i++) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, tmp[i]);
				DrawHex(face_metadata[focused_object_idx][focused_face_idx], x+3, WIN_H-y-95+deltay, 32, 128);
			}
		}
	}
}

void PointCloudView::AddGunVertex(glm::vec3 v) {
	curr_polygon.push_back(v);
	if (curr_polygon.size() == 1 && polygons.size() == 0) {
		bb_ub = v; bb_lb = v;
	}
	bb_ub.x = std::max(bb_ub.x, v.x);
	bb_ub.y = std::max(bb_ub.y, v.y);
	bb_ub.z = std::max(bb_ub.z, v.z);
	bb_lb.x = std::min(bb_lb.x, v.x);
	bb_lb.y = std::min(bb_lb.y, v.y);
	bb_lb.z = std::min(bb_lb.z, v.z);
	sum += v;
	num_verts ++;
}

void PointCloudView::Clear() {
	polygons.clear();
	curr_polygon.clear();
	bb_ub = glm::vec3(-1e10, -1e10, -1e10);
	bb_lb = glm::vec3( 1e10,  1e10,  1e10);
	sum   = glm::vec3(    0,     0,     0);
	should_append = true;
	num_verts = 0;
	focused_face_idx   = -999;
	focused_object_idx = -999;
	object_polygon_ranges.clear();
	object_metadata.clear();
	face_metadata.clear();
}

void PointCloudView::RequestClear() {
	should_clear = true;
}

void PointCloudView::BeginNewPolygon() {
	if (curr_polygon.size() > 0) {
		polygons.push_back(curr_polygon);
		curr_polygon.clear();
	}
}

void PointCloudView::WriteToFile(const std::string& file_name) {
	std::ofstream ofs(file_name);
	if (ofs.good() == false) {
		printf("Oh! Could not write to file %s\n", file_name.c_str());
		return;
	}

	const int n = int(polygons.size())+1;
	ofs.write((const char*)&n, sizeof(int));

	for (int i=0; i<n; i++) {
		std::vector<glm::vec3>* p;
		if (i == n-1) { p = &(curr_polygon); }
		else p = &(polygons[i]);

		const int m = int(p->size());
		ofs.write((const char*)&m, sizeof(int));
		for (int j=0; j<m; j++) {
			glm::vec3 v = p->at(j);
			ofs.write((const char*)&v.x, sizeof(glm::vec3));
		}
	}
	ofs.close();
	g_logview->AppendEntry("Saved data to " + file_name + ", " + std::to_string(n+1) + " polygons");
}

void PointCloudView::ReadFromFile(const std::string& file_name) {
	std::ifstream ifs(file_name);
	if (ifs.good() == false) {
		g_logview->AppendEntry("Could not load data from " + file_name);
		return;
	}

	Clear();
	polygons.clear();
	curr_polygon.clear();

	std::vector<char> dummy;
	for (int i=0; i<128; i++) { dummy.push_back(char(i)); }
	object_metadata.push_back(dummy);

	face_metadata.resize(1);

	int n;
	ifs.read((char*)&n, sizeof(int));
	for (int i=0; i<n; i++) {
		BeginNewPolygon();
		int m;
		ifs.read((char*)&m, sizeof(int));
		for (int i=0; i<m; i++) {
			glm::vec3 v;
			ifs.read((char*)&v.x, sizeof(glm::vec3));
			AddGunVertex(v);
		}
		dummy.clear();
		for (int j=0; j<128; j++) { dummy.push_back(char((i+j)&0xFF)); }
		face_metadata[0].push_back(dummy);
	}

	g_logview->AppendEntry("Loaded data from " + file_name + ", " + std::to_string(n) + " polygons");

	glm::vec3 extent = bb_ub - bb_lb;
	SetCrystalBallView(extent);
	object_polygon_ranges.push_back(std::make_pair(0, int(polygons.size())));
}

void PointCloudView::ChangeSpeedMultiplier(const float mult) {
	speed_multiplier *= mult;
	if (speed_multiplier <= 1) speed_multiplier = 1;
	else if (speed_multiplier >= 8192) speed_multiplier = 8192;
}

glm::vec3 GunCoordToOpenglCoord(const glm::vec3& p) {
	glm::vec3 ret;
	ret.x =  p.x;
	ret.y =  p.z;
	ret.z = -p.y;
	return ret;
}

void PointCloudView::SaveXYWH() {
	x0 = x; y0 = y; w0 = w; h0 = h;
}

void PointCloudView::LoadXYWH() {
	x = x0; y = y0; w = w0; h = h0;
}

void PointCloudView::Maximize() {
	x = 4; y = 50; w = WIN_W - 8; h = WIN_H - 52;
}

void PointCloudView::SetVertexVisibility(int idx, bool v) {
	if (idx >= visited.size()) { visited.resize(idx+1); }
	visited[idx] = v;
}

void PointCloudView::CycleToNextFace()       { CycleOneFace( 1); }
void PointCloudView::CycleToPreviousFace()   { CycleOneFace(-1); }
void PointCloudView::CycleToNextObject()     { CycleObject( 1); }
void PointCloudView::CycleToPreviousObject() { CycleObject(-1); }

void PointCloudView::CycleOneFace(int inc) {
	if (object_polygon_ranges.size() < 1) return;
	if (inc != 0 && inc != 1 && inc != -1) return;
	const int nf = object_polygon_ranges[focused_object_idx].second -
	               object_polygon_ranges[focused_object_idx].first;
	int noi = focused_object_idx;
	int nfi = focused_face_idx + inc;
	if (nfi < 0) {
		noi--;
		if (noi < 0) noi = int(object_polygon_ranges.size()) - 1;
		nfi = object_polygon_ranges[noi].second - object_polygon_ranges[noi].first -1;
	} else if (nfi >= nf) {
		noi++;
		if (noi >= int(object_polygon_ranges.size())) noi = 0;
		nfi = 0;
	}
	focused_object_idx = noi;
	focused_face_idx   = nfi;
}

void PointCloudView::CycleObject(int inc) {
	if (object_polygon_ranges.size() < 1) return;
	int noi = focused_object_idx;
	noi += inc;
	if (noi >= int(object_polygon_ranges.size())) noi = 0;
	else if (noi < 0) { noi = int(object_polygon_ranges.size()) - 1; }
	focused_object_idx = noi;
	focused_face_idx = 0;
}