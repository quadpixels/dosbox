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
void mouse(int, int, int, int);
const int WIN_W = 640, WIN_H = 720;

MemView::MemView(int _disp_w, int _disp_h) {
	bytes2pixel = new BytesToRG();

	disp_w = _disp_w; disp_h = _disp_h;
	bytes_w = disp_w * bytes2pixel->NumBytesPerPixel(); bytes_h = disp_h;

	bytes = new unsigned char[bytes_w * bytes_h];
	pixels = new unsigned char[disp_w * disp_h * bytes2pixel->NumPixelDataChannels()];
	memset(bytes, 0x00, bytes_w * bytes_h);
	memset(pixels,0x00, disp_w * disp_h);

	top = 24; left = 4;

	LoadDummy();
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
		glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *ch);
	}
	glColor3f(1, 1, 1);
	glBegin(GL_LINE_LOOP);
	const int x0 = left-1, x1 = left+disp_w+1, y0 = WIN_H - (top-1), y1 = WIN_H - (top+disp_h+1);
	glVertex2i(x0, y0);
	glVertex2i(x0, y1);
	glVertex2i(x1, y1);
	glVertex2i(x1, y0);
	glEnd();
}

void MemView::Pan(int delta) {
	unsigned int* a = &(view_window.address);
	long x = long(*a) + delta;
	if (x < 0) x = 0;
	else if (x > 0xFFFFFFFFU) x = 0xFFFFFFFFU;
	*a = x;
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
	SetInfoAddressChangedManually();
}

MemView* g_memview;

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

pthread_t g_thread;
int g_argc; char** g_argv;
void* MyDebugInit(void* x);
char g_message[200];

void StartMyDebugThread(int argc, char** argv) {
	g_argc = argc; g_argv = argv;
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
	XInitThreads();
	glOrtho(0, WIN_W, 0, WIN_H, 0, 1); // Use windowPos coordinate system

#ifdef MEM_VIZ_STANDALONE
#endif

	g_memview = new MemView(256, 512);
	g_memview->SetInfoAddressChangedManually();

	glewInit();
	glutMainLoop();
	return NULL;
}

ViewWindow* MyDebugViewWindow() { return &(g_memview->view_window); }

void update() {
	glutPostRedisplay();
}

void render() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.2f, 0.2f, 0.4f, 1.0f);

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
	case '[': g_memview->Pan(-65536); break;
	case ']': g_memview->Pan( 65536); break;
	case '-': g_memview->Zoom(-1); break; // Zoom out
	case '=': g_memview->Zoom(1); break; // Zoom in
	}
}

void keyboard2(int key, int x, int y) {
	switch (key) {
	case GLUT_KEY_PAGE_UP: g_memview->Pan(-1024); break;
	case GLUT_KEY_PAGE_DOWN: g_memview->Pan(1024); break;
	}
}

void mouse(int button, int state, int x, int y) {
	snprintf(g_message, sizeof(g_message), "Mouse: (%d,%d,button=%d,state=%d)", x, y, button, state);
	//printf("%s\n", g_message);
	if (button == 3) { // Scroll up
		g_memview->PanLines(-16);
	} else {
		g_memview->PanLines(16);
	}
}

void motion(int x, int y) {
	snprintf(g_message, sizeof(g_message), "Motion: (%d,%d)", x, y);
	//printf("%s\n", g_message);
}

#ifdef MEM_VIZ_STANDALONE
int main(int argc, char** argv) {
  //MyDebugInit(argc, argv);
	StartMyDebugThread(argc, argv);
	pthread_join(g_thread, NULL);
}
#endif
