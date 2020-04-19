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

// Build standalone version:
// g++-8 mydebug.cpp -lGL -lGLEW -lglut -I. -DMEM_VIZ_STANDALONE -lpthread -lX11

GLEW_FUN_EXPORT PFNGLWINDOWPOS2IPROC glWindowPos2i; // Keep Eclipse CDT happy

// Global Variables

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

void render();
void update();
void keyboard(unsigned char, int, int);
void keyboard2(int, int, int);
void motion(int x, int y);
void motion_passive(int x, int y);
void mouse(int, int, int, int);
const int WIN_W = 640, WIN_H = 720;

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

void MemView::Pan(int delta) {
	const unsigned stride = GetLineStride();
	unsigned int* a = &(view_window.address);
	const unsigned ub = (IsBufferAvailable() ? data_buffer.size() : 0xFFFFFFFFU);
	long x = long(*a) + delta;
	if (x < 0) x = 0;
	else if (x > ub) x = ub;
	*a = (x / stride) * stride;
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
	fread(data_buffer.data(), 1, len, f);
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
}

pthread_t g_thread;
int g_argc; char** g_argv;
void* MyDebugInit(void* x);

void StartMyDebugThread(int argc, char** argv) {
	g_argc = argc; g_argv = argv;
	g_memview = new MemView(256, 512);
	g_memview->SetInfoAddressChangedManually();

	// Load file
	// Set data source
	if (argc > 1) {
		g_memview->LoadBufferFile(argv[1]);
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
	glutSpecialFunc(keyboard2);
	glutMouseFunc(mouse);
	glutMotionFunc(motion);
	glutPassiveMotionFunc(motion_passive);
	XInitThreads();
	glOrtho(0, WIN_W, 0, WIN_H, 0, 1); // Use windowPos coordinate system

#ifdef MEM_VIZ_STANDALONE
#endif

	glewInit();
	glutMainLoop();
	return NULL;
}

ViewWindow* MyDebugViewWindow() { return &(g_memview->view_window); }

void update() {

	// Pop
	const int l = g_mousewheel_accum.PopLines();
	if (l != 0) g_memview->PanLines(l);

	glutPostRedisplay();
}

void render() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.2f, 0.2f, 0.4f, 1.0f);
	glDisable(GL_LIGHTING);

	g_memview->Render();

	glWindowPos2i(0, 4);
	glColor3f(1, 1, 1);
	const char* ch = g_message;
	while (*ch != 0) {
		glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *ch);
		ch++;
	}

	glutSwapBuffers();
}

void keyboard(unsigned char key, int x, int y) {
	switch (key) {
#ifdef MEM_VIZ_STANDALONE
	case 27: exit(0); break;
#endif
	case '[': g_memview->PanLines(-32); break;
	case ']': g_memview->PanLines( 32); break;
	case '-': g_memview->Zoom(-1); break; // Zoom out
	case '=': g_memview->Zoom(1); break; // Zoom in
#ifdef MEM_VIZ_STANDALONE
	case 'r': g_memview->LoadDummy(); break;
#endif
	case 'r': g_memview->ClearHistograms(); break;
	}
}

void keyboard2(int key, int x, int y) {
	switch (key) {
	case GLUT_KEY_PAGE_UP: g_memview->Pan(-1024); break;
	case GLUT_KEY_PAGE_DOWN: g_memview->Pan(1024); break;
	}
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
