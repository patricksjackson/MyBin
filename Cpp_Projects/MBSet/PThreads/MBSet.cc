/** @file MBSet.cc
 *  Calculate and display the Mandelbrot set
 *  ECE4893/8893 final project, Fall 2011
 *  Patrick Jackson
 */

#include <string.h>

#include <GL/glut.h>
#include <GL/glext.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "complex.h"

using namespace std;

#define DEBUG //!< Un comment to print debug statements
#ifdef DEBUG
#include <sys/time.h>
#include <iomanip>
#endif

#define NTHREADS 16

// Defaults
uint32_t init_width = 512;
uint32_t init_height = 512;
double init_XMin = -2.0;
double init_YMin = -1.5;
double init_XMax = 1.0;
double init_YMax = 1.5;
bool click = false;
int click_XMin;
int click_XMax;
int click_YMin;
int click_YMax;
int maxIt = 2000;     //!< Max iterations for the set computations

GLfloat* default_palette;

//! Structure to hold all information necessary for a single viewport
struct MBWindow {
	double XMin;           //!< Minimum X value of the window
	double XMax;           //!< Maximum X value of the window
	double YMin;           //!< Minimum Y value of the window
	double YMax;           //!< Maximum Y value of the window
	uint32_t width;    //!< Width of the window in pixels
	uint32_t height;   //!< Height of the window in pixels
	uint32_t maxIters; //!< Maximum iterations for the computations
	uint16_t* Iters;   //!< Holds the iterations
	GLfloat* palette;      //!< Color palette for this window
};

MBWindow window_list[100]; //!< List containing all the drawn windows
size_t window = 0;  //!< Index of current working window
pthread_mutex_t win_mutex; //!< Mutex for accessing the current window
pthread_barrier_t calc_barrier; //!< Barrier for synchronization between the rank0 threads

/** Iterate 2 Points
 *  returns the number of iterations below max_iter that it took to reach
 *  the threshold
 *
 *  @param a Real component of original point
 *  @param b Imaginary component of original point
 *  @param c Address to save the escape value of the original point
 *  @param a2 Real component of second point
 *  @param b2 Imaginary component of second point
 *  @param c2 Address to save the escape value of the second point
 *  @param max_iter Maximum number of iterations to run
 */
void iterate_2_points(double a, double b, uint16_t* c, double a2, double b2, uint16_t* c2, uint32_t max_iter)
{
	double r, i, rr, ii;
	r = i = rr = ii = 0.0;

	double r2, i2, rr2, ii2;
	r2 = i2 = rr2 = ii2 = 0.0;

	bool orig_finished = false, sec_finished = false;

	do {
		// Run a single step of the algorithm and check
		i = 2 * r * i + b;
		r = rr - ii + a;
		rr = r * r;
		ii = i * i;

		i2 = 2 * r2 * i2 + b2;
		r2 = rr2 - ii2 + a2;
		rr2 = r2 * r2;
		ii2 = i2 * i2;

		if ( (rr + ii) >= 4.0 ) {
			*c = max_iter;
			if ( sec_finished )
				return;
			r = i = rr = ii = 0.0;
			a = b = 0.0;
			orig_finished = true;
		}
		if ( (rr2 + ii2) >= 4.0 ) {
			*c2 = max_iter;
			if ( orig_finished )
				return;
			r2 = i2 = rr2 = ii2 = 0.0;
			a2 = b2 = 0.0;
			sec_finished = true;
		}
			
	} while ( --max_iter );

	if ( !orig_finished )
		*c = 0;
	if ( !sec_finished )
		*c2 = 0;

	return ;
}

/** Iterate Point
 *  returns the number of iterations below max_iter that it took to reach
 *  the threshold
 *
 *  @param a Real component of original point
 *  @param b Imaginary component of original point
 *  @param max_iter Maximum number of iterations to run
 *  @return The number of iterations left until reaching the max
 */
uint16_t iterate_point(double a, double b, uint32_t max_iter)
{
	double r, i, rr, ii;
	r = i = rr = ii = 0.0;

	do {
		// Run a single step of the algorithm and check
		i = 2 * r * i + b;
		r = rr - ii + a;
		rr = r * r;
		ii = i * i;
		if ( (rr + ii) >= 4.0 )
			break;
	} while ( --max_iter );

	return max_iter;
}

/** Iterate Point Complex
 *  this version of iteration uses the complex class. It is appx 4 times slower
 *
 *  @param a Real component of original point
 *  @param b Imaginary component of original point
 *  @param max_iter Maximum number of iterations to run
 *  @return The number of iterations left until reaching the max
 */
uint16_t iterate_point_complex(double a, double b, uint32_t max_iter)
{
	Complex c(a, b);
	Complex z(0, 0);

	do {
		z = z*z;
		z = z + c;
		if (z.Mag2() > 16.0)
			break;
	} while (--max_iter);
	return max_iter;
}

struct compute_thread_struct_t {
	int total_tasks;  //!< Informs thread of the total number of tasks in the system
	int thread_rank;  //!< Informs thread of its rank
	MBWindow* w;      //!< Pointer to the window to update
};

/** Compute Thread
 *  runs the computation for a subset of the mandelbrot image
 */
void* compute_thread(void *info)
{
	compute_thread_struct_t* i = (compute_thread_struct_t*) info;
	int numtasks, rank;
	numtasks = i->total_tasks;
	rank = i->thread_rank;
	MBWindow* w = i->w;

	// Figure out number of rows and starting row for calculations
	uint32_t height = w->height / numtasks;
	if (0 != (w->height % numtasks))
		height++;
	uint32_t start = height * rank;
	if (rank == numtasks-1) { // Scale last task down to not overshoot end
		height = w->height - start;
	}

#ifdef DEBUG
	cout << "Rank " << rank << " started." << start << endl;
#endif

	// Allocate space
	uint16_t* iters = (uint16_t*) malloc(sizeof(uint16_t) * w->width * height);
	size_t index = 0;

	// Run through the calculations
	double imag, real, real2;
	for (uint32_t r = start; r < start+height; r++) {
		imag = (((double) r) / ((double) w->height - 1)) * (w->YMax - w->YMin) + w->YMin;
		for (uint32_t c = 0; c < w->width; c+=2) {
			real = (((double) c) / ((double) w->width - 1)) * (w->XMax - w->XMin) + w->XMin;
			real2 = (((double) c+1) / ((double) w->width - 1)) * (w->XMax - w->XMin) + w->XMin;
			//iters[index++] = iterate_point(real, imag, w->maxIters);
			iterate_2_points(real, imag, &iters[index], real2, imag, &iters[index+1], w->maxIters);
			index += 2;
		}
	}
#ifdef DEBUG
	cout << "Rank " << rank << " with " << start << endl;
#endif

	memcpy(&w->Iters[w->width * start], iters, sizeof(uint16_t) * w->width * height);

	free( iters );

	pthread_exit( NULL );
}

/** Mandelbrot Compute Threads
 *  Computes the window area with threads
 *
 *  @param w MBWindow pointer that needs to be computed
 */
void mandelbrot_compute_threads(MBWindow* w)
{
	// Skip if already calculated this window
	if (NULL != w->Iters)
		return;
	w->Iters = (uint16_t*) malloc(sizeof(uint16_t) * w->width * w->height);
#ifdef DEBUG
	cout << "Calculating..." << endl;
	struct timeval t;
	gettimeofday(&t, 0);
#endif
	// Fill a list with all the arguments to the threads
	compute_thread_struct_t* info = (compute_thread_struct_t*) malloc(sizeof(compute_thread_struct_t) * NTHREADS);
	for (int i = 0; i < NTHREADS; i++) {
		info[i].total_tasks = NTHREADS;
		info[i].thread_rank = i;
		info[i].w = w;
	}
	// Create the threads
	pthread_t* p = (pthread_t*) malloc(sizeof(pthread_t) * NTHREADS);
	for (int i = 0; i < NTHREADS; i++) {
		pthread_create(&p[i], 0, compute_thread, (void*)&info[i]); 
	}
	// Wait until threads finish
	for (int i = 0; i < NTHREADS; i++) {
		pthread_join(p[i], NULL);
	}
	// Free memory
	free( info );
	free( p );
#ifdef DEBUG
	cout << "Finished calculating" << endl;
	struct timeval t2;
	gettimeofday(&t2, 0);
	uint32_t time = (t2.tv_sec - t.tv_sec) * 1000000 + (t2.tv_usec - t.tv_usec);
	cout << "Calculation time: " << setw(6) << ((float)time) / 1000000 << endl;
#endif

}

void mandelbrot_compute(MBWindow* w)
{
	// Skip if already calculated this window
	if (NULL != w->Iters)
		return;
#ifdef DEBUG
	cout << "Calculating..." << endl;
	struct timeval t;
	gettimeofday(&t, 0);
#endif
	w->Iters = (uint16_t*) malloc(sizeof(uint16_t) * w->width * w->height);
	double real, imag;
	for (uint32_t r = 0; r < w->height; r++) {
		imag = (((double) r) / ((double) w->height - 1)) * (w->YMax - w->YMin) + w->YMin;
		for (uint32_t c = 0; c < w->width; c++) {
			real = (((double) c) / ((double) w->width - 1)) * (w->XMax - w->XMin) + w->XMin;
			w->Iters[w->width * r + c] = iterate_point(real, imag, w->maxIters);
		}
	}
#ifdef DEBUG
	cout << "Finished calculating" << endl;
	struct timeval t2;
	gettimeofday(&t2, 0);
	uint32_t time = (t2.tv_sec - t.tv_sec) * 1000000 + (t2.tv_usec - t.tv_usec);
	cout << "Calculation time: " << setw(6) << ((float)time) / 1000000 << endl;
#endif
}

/** Display
 *  re-draw the image to the screen. Clear old screen, draw MBset,
 *  swap buffers.
 */
void display(void)
{
#ifdef DEBUG
	static int pass;
	cout << "Displaying pass " << ++pass << endl;
#endif

	// Get a pointer to the current window
	MBWindow* w = &window_list[window];

	if (NULL == w->Iters) {
		// Compute the window
		mandelbrot_compute_threads(w);
	}

	// Clear all the colors
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(1.0, 1.0, 1.0, 1.0);

	// Clear the matrix
	glLoadIdentity();

	// Set the viewing transformation
	gluLookAt(0.0, 0.0, 5.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
	
	// Correct for 1 pixel discrepency
	glTranslatef(1, 0, 0);

	// Iterate over calculated image and display points
	glBegin( GL_POINTS );
	for (uint32_t r = 0; r < w->height; r++) {
		for (uint32_t c = 0; c < w->width; c++) {
			glColor3fv(&w->palette[3 * w->Iters[w->width * r + c]]);
			glVertex2d(c, r);
		}
	}
	if (click) {
		glColor3f(1.0, 0.0, 0.0);
		for (int i = click_XMin; i != click_XMax; i += (click_XMax - click_XMin) / abs(click_XMax - click_XMin)) {
			glVertex2d(i, w->height - click_YMin);
			glVertex2d(i, w->height - click_YMax);
		}
		for (int i = click_YMin; i != click_YMax; i += (click_YMax - click_YMin) / abs(click_YMax - click_YMin)) {
			glVertex2d(click_XMin, w->height - i);
			glVertex2d(click_XMax, w->height - i);
		}
	}
	glEnd();

	// Double buffering
	glutSwapBuffers();
}

/** Set Max Iterations
 *  changes the number of iterations we will attempt before concluding if something is part of the set
 *
 *  @param num New value for maxIt
 */
void setmaxIt( int num )
{
	maxIt = num;
	// Initialize default palette
	if ( NULL != default_palette )
		free( default_palette );
	default_palette = (GLfloat*) malloc(sizeof(GLfloat) * (maxIt+1) * 3);
	for (int i = 0; i < maxIt*3; i++)
		default_palette[i] = ((GLfloat)rand()) / RAND_MAX;
	// Mark Black and white
	for (int i = 0; i < 3; i++) {
		default_palette[i] = 0.0;
		default_palette[3*maxIt-i+2] = 1.0;
		default_palette[3*maxIt-i-1] = 1.0;
		default_palette[3*maxIt-(2*i)-1] = 1.0;
		default_palette[3*maxIt-(3*i)-1] = 1.0;
	}
}

/** Initialize
 *  performs opengl initialization functions.
 */
void init()
{
	// Set bg color
#ifdef DEBUG
	// Draw red bg to check for missed pixels
	glClearColor(1.0, 0.0, 0.0, 1.0);
#else
	glClearColor(1.0, 1.0, 1.0, 1.0);
#endif
	glShadeModel(GL_FLAT);

	setmaxIt( 2000 );

	// Initialize current window defaults
	MBWindow* w = &window_list[window];
	w->XMin = init_XMin;
	w->YMin = init_YMin;
	w->XMax = init_XMax;
	w->YMax = init_YMax;
	w->maxIters = maxIt;
	w->width = init_width;
	w->height = init_height;
	w->Iters = NULL;
	w->palette = default_palette;
}

/** Fix Aspect Ratio
 *  sets the window to the correct dimensions based on the aspect ratio of the window
 *
 *  @param w Pointer to the window object to adjust
 */
void fix_aspect( MBWindow* w )
{
	// Scale Min and Max points for correct aspect ratio
	// shouldn't change anything unless window is not a square
	double pix_scale = ((double)w->height) / w->width;
	double point_scale = (w->YMax - w->YMin) / (w->XMax - w->XMin);
	if (pix_scale != point_scale) {
		double dy = w->YMax - w->YMin;
		double dx = w->XMax - w->XMin;
		double x_pix_size = dx / ((double)w->width);
		double y_pix_size = dy / ((double)w->height);
		if (x_pix_size < y_pix_size) {
			// Modify X vals
			double mid = (w->XMax + w->XMin) / 2.0;
			w->XMin = mid - (y_pix_size * w->width / 2.0);
			w->XMax = mid + (y_pix_size * w->width / 2.0);
		}
		else if (x_pix_size > y_pix_size) {
			// Modify Y vals
			double mid = (w->YMax + w->YMin) / 2.0;
			w->YMin = mid - (x_pix_size * w->height / 2.0);
			w->YMax = mid + (x_pix_size * w->height / 2.0);
		}
	}
}

/** Reshape
 *  when the window is resized, redraw the image
 *
 *  @param w Width of new window
 *  @param h Height of new window
 */
void reshape(int w, int h)
{
#ifdef DEBUG
	cout << "Reshape: (" << w << ", " << h << ")" << endl;
#endif
	// Get pointer to current window object
	pthread_mutex_lock(&win_mutex);
	memcpy(&window_list[window+1], &window_list[window], sizeof(MBWindow));
	window++;
	MBWindow* win = &window_list[window];
	win->width = w;
	win->height = h;
	win->Iters = NULL;

	fix_aspect( win );

	pthread_mutex_unlock(&win_mutex);
	// Resize viewport and ortho projection
	glViewport(0,0, (GLsizei)w, (GLsizei)h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, (GLdouble)w, (GLdouble)0.0, h, (GLdouble)-w, (GLdouble)w);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void mouse(int button, int state, int x, int y)
{ // Your mouse click processing here
  // state == 0 means pressed, state != 0 means released
  // Note that the x and y coordinates passed in are in
  // PIXELS, with y = 0 at the top.
#ifdef DEBUG
	cout << "Mouse: " << button << " - " << state << " - (" << x << ", " << y << ")" << endl;
#endif
	static bool firstvalid = false;
	if (0 == state) {
		firstvalid = true;
		click_XMin = x;
		click_YMin = y;
		click_XMax = x;
		click_YMax = y;
		click = true;
	}
	if (state && firstvalid) {
		click = false;

		// Calculate pixel coordinates
		int min = (abs(x - click_XMin) < abs(y - click_YMin)) ? x-click_XMin : y-click_YMin;
		if (0 == min)
			return;
		min = abs(min);
		click_XMax = click_XMin + (min * ((x - click_XMin) / abs(x-click_XMin)));
		click_YMax = click_YMin + (min * ((y - click_YMin) / abs(y-click_YMin)));

		pthread_mutex_lock(&win_mutex);
		MBWindow* w = &window_list[window+1];
		MBWindow* wtmp = &window_list[window];
		w->YMin = (((double)wtmp->height - click_YMax) / ((double) wtmp->height)) * (wtmp->YMax - wtmp->YMin) + wtmp->YMin;
		w->YMax = (((double)wtmp->height - click_YMin) / ((double) wtmp->height)) * (wtmp->YMax - wtmp->YMin) + wtmp->YMin;
		w->XMin = (((double)click_XMin) / ((double) wtmp->width)) * (wtmp->XMax - wtmp->XMin) + wtmp->XMin;
		w->XMax = (((double)click_XMax) / ((double) wtmp->width)) * (wtmp->XMax - wtmp->XMin) + wtmp->XMin;
		
		// Ensure that we have the min and max set up as lower left and upper right corners
		if (w->YMax < w->YMin) {
			// Swap
			double tmp = w->YMax;
			w->YMax = w->YMin;
			w->YMin = tmp;
		}
		if (w->XMax < w->XMin) {
			// Swap
			double tmp = w->XMax;
			w->XMax = w->XMin;
			w->XMin = tmp;
		}

		// Inherit attributes from old window
		w->width = wtmp->width;
		w->height = wtmp->height;
		w->maxIters = wtmp->maxIters;
		w->Iters = NULL;
		w->palette = wtmp->palette;

		fix_aspect(w);
		window++;
		pthread_mutex_unlock(&win_mutex);
		glutPostRedisplay();
	}
}

void motion(int x, int y)
{
#ifdef DEBUG
	cout << "Motion: (" << x << ", " << y << ")" << endl;
#endif
	int min = (abs(x - click_XMin) < abs(y - click_YMin)) ? x-click_XMin : y-click_YMin;
	if (0 == min)
		return;
	min = abs(min);
	click_XMax = click_XMin + (min * ((x - click_XMin) / abs(x-click_XMin)));
	click_YMax = click_YMin + (min * ((y - click_YMin) / abs(y-click_YMin)));
	glutPostRedisplay();
}

void keyboard(uint8_t c, int x, int y)
{ 
#ifdef DEBUG
	cout << "Keyboard: (" << x << ", " << y << ") " << c << endl;
#endif
	// Switch on keyboard command
	switch ( c ) {
		case 'b':	
			if (window > 0) // Restrict to positive values
				window--;
			glutPostRedisplay();
			break;

		case '+':
			setmaxIt( maxIt * 2 );
			window_list[window].maxIters *= 2;
			free( window_list[window].Iters );
			window_list[window].Iters = NULL;
			glutPostRedisplay();
			break;

		case '-':
			setmaxIt( maxIt / 2 );
			glutPostRedisplay();
			break;

		default:	
			break;
	}
}

int main(int argc, char** argv)
{

	// Init mutex
	pthread_mutex_init(&win_mutex, NULL);
	//pthread_barrier_init(&calc_barrier, NULL, 2);


	// Initialize glut to default window size and display mode
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(init_width, init_height);
	glutInitWindowPosition(100, 100);
	glutCreateWindow("MBSet");

	// Run initialization for opengl
	init();

	// Set callbacks
	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutMouseFunc(mouse);
	glutKeyboardFunc(keyboard);
	glutMotionFunc(motion);

	// Start main loop
	glutMainLoop();

	return 0;
}

