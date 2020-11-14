#include <windows.h>

#include "string.h"

#include <gl/glew.h> // http://glew.sourceforge.net/
#include <gl/wglew.h>

#include <FreeImage.h> // http://freeimage.sourceforge.net/

#include <glm/glm.hpp> // http://glm.g-truc.net/
#include <glm/gtx/rotate_vector.hpp>

using namespace glm;

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "glew32.lib")

#pragma comment(lib, "FreeImage.lib")

// ----------------------------------------------------------------------------------------------------------------------------

void DisplayError(char *ErrorText);
void DisplayInfo(char *InfoText);
bool DisplayQuestion(char *QuestionText);

// ----------------------------------------------------------------------------------------------------------------------------

class CTexture
{
protected:
	GLuint TextureID;

public:
	CTexture();
	~CTexture();

	operator GLuint ();

	void Delete();
	bool LoadTexture2D(char *Texture2DFileName);
};

// ----------------------------------------------------------------------------------------------------------------------------

class CShaderProgram
{
public:
	GLuint *UniformLocations;

protected:
	GLuint VertexShader, FragmentShader, Program;

public:
	CShaderProgram();
	~CShaderProgram();

	operator GLuint ();

	void Delete();
	bool Load(char *VertexShaderFileName, char *FragmentShaderFileName);

protected:
	GLuint LoadShader(GLenum Type, char *ShaderFileName);
	void SetDefaults();
};

// ----------------------------------------------------------------------------------------------------------------------------

class CCamera
{
protected:
	mat4x4 *View;

public:
	vec3 X, Y, Z, Reference, Position;

	CCamera();
	~CCamera();

	void CalculateViewMatrix();
	void LookAt(vec3 Reference, vec3 Position, bool RotateAroundReference = false);
	void Move(vec3 Movement);
	vec3 OnKeys(BYTE Keys, float FrameTime);
	void OnMouseMove(int dx, int dy);
	void OnMouseWheel(short zDelta);
	void SetViewMatrixPointer(mat4x4 *View);
};

// ----------------------------------------------------------------------------------------------------------------------------

class COpenGLRenderer
{
protected:
	int Width, Height;
	mat4x4 Model, View, Projection;

	CTexture Texture;
	CShaderProgram Shader;

	vec2 *TexCoords;
	vec3 *Normals, *Vertices;

public:
	bool ShowAxisGrid, Stop;

public:
	COpenGLRenderer();
	~COpenGLRenderer();

	bool Init();
	void Render(float FrameTime);
	void Resize(int Width, int Height);
	void Destroy();
};

// ----------------------------------------------------------------------------------------------------------------------------

class CWnd
{
protected:
	char *WindowName;
	bool FullScreen, DeFullScreened;
	DEVMODE DevMode;
	HWND hWnd;
	HDC hDC;
	int Samples;
	HGLRC hGLRC;
	int Width, Height, WidthD2, HeightD2;
	DWORD Start, Begin;
	POINT LastCurPos;
	bool MouseGameMode, KeyBoardFocus, MouseFocus;

public:
	CWnd();
	~CWnd();

	bool Create(HINSTANCE hInstance, char *WindowName, int Width, int Height, bool FullScreen = false, int Samples = 4, bool CreateForwardCompatibleContext = false, bool DisableVerticalSynchronization = true);
	void Show(bool MouseGameMode = false, bool Maximized = false);
	void MsgLoop();
	void Destroy();

protected:
	void GetCurPos(int *cx, int *cy);
	void SetCurPos(int cx, int cy);
	void SetCurAccToMouseGameMode();
	void SetMouseFocus();
	void StartFPSCounter();

public:
	void OnKeyDown(UINT nChar);
	void OnKillFocus();
	void OnLButtonDown(int cx, int cy);
	void OnMouseMove(int cx, int cy);
	void OnMouseWheel(short zDelta);
	void OnPaint();
	void OnRButtonDown(int cx, int cy);
	void OnSetFocus();
	void OnSize(int sx, int sy);
};

// ----------------------------------------------------------------------------------------------------------------------------

LRESULT CALLBACK WndProc(HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam);
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR sCmdLine, int iShow);
