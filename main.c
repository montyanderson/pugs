#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/stat.h>
#include <string.h>
#include <GL/gl.h>
#include <GL/glut.h>
#include <curl/curl.h>
#include <jpeglib.h>
#include <syslog.h>
#include "jsmn.h"

struct string {
	char *ptr;
	size_t len;
};

size_t writeBody(void *ptr, size_t size, size_t nmemb, struct string *string) {
	size_t newLen = string->len + (size * nmemb);

	string->ptr = realloc(string->ptr, newLen + 1);

	if(string->ptr == NULL) {
		fprintf(stderr, "realloc() failed\n");
		exit(EXIT_FAILURE);
	}

	memcpy(string->ptr + string->len, ptr, size * nmemb);

	string->ptr[newLen] = 0;
	string->len = newLen;

	return size * nmemb;
}

struct string getUrl(char *url) {
	CURL *curl = curl_easy_init();

	struct string body;

	body.ptr = NULL;
	body.len = 0;

	CURLcode res;

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeBody);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

	res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	if(res) {
		printf("Failed to get url (code: %u) '%s'\n", res, url);
	}

	return body;
}

void removeCharacter(char *str, char c) {
	size_t len = strlen(str);

	for(size_t i = 0; i < len; i++) {
		if(str[i] == c) {
			memmove(str + i, str + 1 + i, len - i);
		}
	}

	str[len] = 0;
}

char *getPugUrl() {
	struct string body = getUrl("https://dog.ceo/api/breed/pug/images/random");

	jsmn_parser p;
	jsmntok_t t[128];

	jsmn_init(&p);
	size_t r = jsmn_parse(&p, body.ptr, body.len, t, 128);

	int urlIndex = -1;

	for(size_t i = 0; i < r; i++) {
		if(t[i].type == JSMN_STRING) {
			if(memcmp("message", body.ptr + t[i].start, t[i].end - t[i].start) == 0) {
				urlIndex = i + 1;
				break;
			}
		}
	}

	if(urlIndex == -1) {
		fprintf(stderr, "failed to find JSON key 'message'");
		exit(EXIT_FAILURE);
	}

	char *url = malloc(t[urlIndex].end - t[urlIndex].start + 1);

	if(url == NULL) {
		fprintf(stderr, "malloc() failed");
		exit(EXIT_FAILURE);
	}

	memcpy(url, body.ptr + t[urlIndex].start, t[urlIndex].end - t[urlIndex].start);

	url[t[urlIndex].end - t[urlIndex].start] = 0;

	free(body.ptr);

	removeCharacter(url, '\\');

	return url;
}

unsigned char *bmp_buffer;
int width, height;

void getNewPug() {
	char *pugUrl = getPugUrl();

	printf("pugUrl: %s\n", pugUrl);

	struct string image = getUrl(pugUrl);

	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;

	unsigned long bmp_size;
	int row_stride, pixel_size;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);

	jpeg_mem_src(&cinfo, image.ptr, image.len);

	int rc = jpeg_read_header(&cinfo, TRUE);

	if (rc != 1) {
		syslog(LOG_ERR, "File does not seem to be a normal JPEG");
		exit(EXIT_FAILURE);
	}

	jpeg_start_decompress(&cinfo);

	width = cinfo.output_width;
	height = cinfo.output_height;
	pixel_size = cinfo.output_components;

	syslog(LOG_INFO, "Proc: Image is %d by %d with %d components",
		width, height, pixel_size);

	bmp_size = width * height * pixel_size;
	bmp_buffer = (unsigned char*) malloc(bmp_size);

	row_stride = width * pixel_size;

	while (cinfo.output_scanline < cinfo.output_height) {
		unsigned char *buffer_array[1];
		buffer_array[0] = bmp_buffer + \
						   (cinfo.output_scanline) * row_stride;

		jpeg_read_scanlines(&cinfo, buffer_array, 1);

	}
	syslog(LOG_INFO, "Proc: Done reading scanlines");

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	unsigned char *bmp_buffer_flipped = (unsigned char*) malloc(bmp_size);

	for(size_t i = 0; i < height; i++) {
		memcpy(bmp_buffer_flipped + (bmp_size - ((i + 1) * width * pixel_size)), bmp_buffer + (i * width * pixel_size), width * pixel_size);
	}

	free(bmp_buffer);
	bmp_buffer = bmp_buffer_flipped;
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

	glDrawPixels(width, height, GL_RGB, GL_UNSIGNED_BYTE, bmp_buffer);

    glFlush();
}

void mouseMove(int button, int state, int x, int y) {
	if(button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
		getNewPug();
		glutReshapeWindow(width, height);
		display();
		glutPostRedisplay();
	}
}

int main(int argc, char **argv) {
	getNewPug();

	glutInit(&argc, argv);

	glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);

	glutInitWindowSize(width, height);

	glutInitWindowPosition(100,100);

	glutCreateWindow("pugs");

	glClearColor(0.0, 0.0, 0.0, 0.0);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);

	glutDisplayFunc(display);
	glutMouseFunc(mouseMove);

    glutMainLoop();
}
