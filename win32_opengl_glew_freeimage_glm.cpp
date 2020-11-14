#include "win32_opengl_glew_freeimage_glm.h"

// ----------------------------------------------------------------------------------------------------------------------------

void DisplayError(char *ErrorText)
{
	MessageBox(NULL, ErrorText, "Error", MB_OK | MB_ICONERROR);
}

void DisplayInfo(char *InfoText)
{
	MessageBox(NULL, InfoText, "Info", MB_OK | MB_ICONINFORMATION);
}

bool DisplayQuestion(char *QuestionText)
{
	return MessageBox(NULL, QuestionText, "Question", MB_YESNO | MB_ICONQUESTION) == IDYES;
}

// ----------------------------------------------------------------------------------------------------------------------------

CString ModuleDirectory, ErrorLog;

bool wgl_context_forward_compatible = false;
int gl_version = 0, gl_max_texture_size = 0, gl_max_texture_max_anisotropy_ext = 0;

// ----------------------------------------------------------------------------------------------------------------------------

CTexture::CTexture()
{
	TextureID = 0;
}

CTexture::~CTexture()
{
}

CTexture::operator GLuint ()
{
	return TextureID;
}

void CTexture::Delete()
{
	glDeleteTextures(1, &TextureID);
	TextureID = 0;
}

bool CTexture::LoadTexture2D(char *Texture2DFileName)
{
	CString FileName = ModuleDirectory + Texture2DFileName;
	CString ErrorText = "Error loading file " + FileName + "! ->";

	FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(FileName);

	if(fif == FIF_UNKNOWN)
	{
		fif = FreeImage_GetFIFFromFilename(FileName);
	}
	
	if(fif == FIF_UNKNOWN)
	{
		ErrorLog.Append(ErrorText + "fif is FIF_UNKNOWN" + "\r\n");
		return false;
	}

	FIBITMAP *dib = NULL;

	if(FreeImage_FIFSupportsReading(fif))
	{
		dib = FreeImage_Load(fif, FileName);
	}
	
	if(dib == NULL)
	{
		ErrorLog.Append(ErrorText + "dib is NULL" + "\r\n");
		return false;
	}

	int Width = FreeImage_GetWidth(dib), oWidth = Width;
	int Height = FreeImage_GetHeight(dib), oHeight = Height;
	int Pitch = FreeImage_GetPitch(dib);
	int BPP = FreeImage_GetBPP(dib);

	if(Width == 0 || Height == 0)
	{
		ErrorLog.Append(ErrorText + "Width or Height is 0" + "\r\n");
		return false;
	}

	if(Width > gl_max_texture_size) Width = gl_max_texture_size;
	if(Height > gl_max_texture_size) Height = gl_max_texture_size;

	if(!GLEW_ARB_texture_non_power_of_two)
	{
		Width = 1 << (int)floor((log((float)Width) / log(2.0f)) + 0.5f); 
		Height = 1 << (int)floor((log((float)Height) / log(2.0f)) + 0.5f);
	}

	if(Width != oWidth || Height != oHeight)
	{
		FIBITMAP *rdib = FreeImage_Rescale(dib, Width, Height, FILTER_BICUBIC);

		FreeImage_Unload(dib);

		if((dib = rdib) == NULL)
		{
			ErrorLog.Append(ErrorText + "rdib is NULL" + "\r\n");
			return false;
		}

		Pitch = FreeImage_GetPitch(dib);
	}

	BYTE *Data = FreeImage_GetBits(dib);

	if(Data == NULL)
	{
		ErrorLog.Append(ErrorText + "Data is NULL" + "\r\n");
		return false;
	}

	GLenum Format = 0;

	if(BPP == 32) Format = GL_BGRA;
	if(BPP == 24) Format = GL_BGR;

	if(Format == 0)
	{
		FreeImage_Unload(dib);
		ErrorLog.Append(ErrorText + "Format is 0" + "\r\n");
		return false;
	}

	if(gl_version < 12)
	{
		if(Format == GL_BGRA) Format = GL_RGBA;
		if(Format == GL_BGR) Format = GL_RGB;

		int bpp = BPP / 8;

		BYTE *line = Data;

		for(int y = 0; y < Height; y++)
		{
			BYTE *pixel = line;

			for(int x = 0; x < Width; x++)
			{
				BYTE Temp = pixel[0];
				pixel[0] = pixel[2];
				pixel[2] = Temp;

				pixel += bpp;
			}

			line += Pitch;
		}
	}

	glGenTextures(1, &TextureID);

	glBindTexture(GL_TEXTURE_2D, TextureID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_version >= 14 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	if(GLEW_EXT_texture_filter_anisotropic)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_max_texture_max_anisotropy_ext);
	}
	
	if(gl_version >= 14 && gl_version <= 21)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	}
	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Width, Height, 0, Format, GL_UNSIGNED_BYTE, Data);
	
	if(gl_version >= 30)
	{
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	FreeImage_Unload(dib);

	return true;
}

// ----------------------------------------------------------------------------------------------------------------------------

CShaderProgram::CShaderProgram()
{
	SetDefaults();
}

CShaderProgram::~CShaderProgram()
{
}

CShaderProgram::operator GLuint ()
{
	return Program;
}

void CShaderProgram::Delete()
{
	delete [] UniformLocations;

	glDetachShader(Program, VertexShader);
	glDetachShader(Program, FragmentShader);

	glDeleteShader(VertexShader);
	glDeleteShader(FragmentShader);

	glDeleteProgram(Program);

	SetDefaults();
}

bool CShaderProgram::Load(char *VertexShaderFileName, char *FragmentShaderFileName)
{
	if(UniformLocations || VertexShader || FragmentShader || Program)
	{
		Delete();
	}

	bool Error = false;

	Error |= ((VertexShader = LoadShader(GL_VERTEX_SHADER, VertexShaderFileName)) == 0);

	Error |= ((FragmentShader = LoadShader(GL_FRAGMENT_SHADER, FragmentShaderFileName)) == 0);

	if(Error)
	{
		Delete();
		return false;
	}

	Program = glCreateProgram();
	glAttachShader(Program, VertexShader);
	glAttachShader(Program, FragmentShader);
	glLinkProgram(Program);

	int Param = 0;
	glGetProgramiv(Program, GL_LINK_STATUS, &Param);

	if(Param == GL_FALSE)
	{
		ErrorLog.Append("Error linking program (%s, %s)!\r\n", VertexShaderFileName, FragmentShaderFileName);

		int InfoLogLength = 0;
		glGetProgramiv(Program, GL_INFO_LOG_LENGTH, &InfoLogLength);
	
		if(InfoLogLength > 0)
		{
			char *InfoLog = new char[InfoLogLength];
			int CharsWritten  = 0;
			glGetProgramInfoLog(Program, InfoLogLength, &CharsWritten, InfoLog);
			ErrorLog.Append(InfoLog);
			delete [] InfoLog;
		}

		Delete();

		return false;
	}

	return true;
}

GLuint CShaderProgram::LoadShader(GLenum Type, char *ShaderFileName)
{
	CString FileName = ModuleDirectory + ShaderFileName;

	FILE *File;

	if(fopen_s(&File, FileName, "rb") != 0)
	{
		ErrorLog.Append("Error loading file " + FileName + "!\r\n");
		return 0;
	}

	fseek(File, 0, SEEK_END);
	long Size = ftell(File);
	fseek(File, 0, SEEK_SET);
	char *Source = new char[Size + 1];
	fread(Source, 1, Size, File);
	fclose(File);
	Source[Size] = 0;

	GLuint Shader;

	Shader = glCreateShader(Type);
	glShaderSource(Shader, 1, (const char**)&Source, NULL);
	delete [] Source;
	glCompileShader(Shader);

	int Param = 0;
	glGetShaderiv(Shader, GL_COMPILE_STATUS, &Param);

	if(Param == GL_FALSE)
	{
		ErrorLog.Append("Error compiling shader %s!\r\n", ShaderFileName);

		int InfoLogLength = 0;
		glGetShaderiv(Shader, GL_INFO_LOG_LENGTH, &InfoLogLength);
	
		if(InfoLogLength > 0)
		{
			char *InfoLog = new char[InfoLogLength];
			int CharsWritten  = 0;
			glGetShaderInfoLog(Shader, InfoLogLength, &CharsWritten, InfoLog);
			ErrorLog.Append(InfoLog);
			delete [] InfoLog;
		}

		glDeleteShader(Shader);

		return 0;
	}

	return Shader;
}

void CShaderProgram::SetDefaults()
{
	UniformLocations = NULL;
	VertexShader = 0;
	FragmentShader = 0;
	Program = 0;
}

// ----------------------------------------------------------------------------------------------------------------------------

CCamera::CCamera()
{
	View = NULL;

	Reference = vec3(0.0f, 0.0f, 0.0f);
	Position = vec3(0.0f, 0.0f, 5.0f);

	X = vec3(1.0f, 0.0f, 0.0f);
	Y = vec3(0.0f, 1.0f, 0.0f);
	Z = vec3(0.0f, 0.0f, 1.0f);
}

CCamera::~CCamera()
{
}

void CCamera::CalculateViewMatrix()
{
	if(View)
	{
		*View = mat4x4(vec4(X.x, Y.x, Z.x, 0.0f), vec4(X.y, Y.y, Z.y, 0.0f), vec4(X.z, Y.z, Z.z, 0.0f), vec4(-dot(X, Position), -dot(Y, Position), -dot(Z, Position), 1.0f));
	}
}

void CCamera::LookAt(vec3 Reference, vec3 Position, bool RotateAroundReference)
{
	this->Reference = Reference;
	this->Position = Position;

	Z = normalize(Position - Reference);
	X = normalize(cross(vec3(0.0f, 1.0f, 0.0f), Z));
	Y = cross(Z, X);

	if(!RotateAroundReference)
	{
		this->Reference = this->Position;
		this->Position += Z * 0.05f;
	}

	CalculateViewMatrix();
}

void CCamera::Move(vec3 Movement)
{
	Reference += Movement;
	Position += Movement;

	CalculateViewMatrix();
}

vec3 CCamera::OnKeys(BYTE Keys, float FrameTime)
{
	float Speed = 5.0f;

	if(Keys & 0x40) // SHIFT
	{
		Speed *= 2.0f;
	}

	float Distance = Speed * FrameTime;

	vec3 Up(0.0f, 1.0f, 0.0f);
	vec3 Right = X;
	vec3 Forward = cross(Up, Right);

	Up *= Distance;
	Right *= Distance;
	Forward *= Distance;

	vec3 Movement;

	if(Keys & 0x01) // W
	{
		Movement += Forward;
	}

	if(Keys & 0x02) // S
	{
		Movement -= Forward;
	}

	if(Keys & 0x04) // A
	{
		Movement -= Right;
	}

	if(Keys & 0x08) // D
	{
		Movement += Right;
	}

	if(Keys & 0x10) // R
	{
		Movement += Up;
	}

	if(Keys & 0x20) // F
	{
		Movement -= Up;
	}

	return Movement;
}

void CCamera::OnMouseMove(int dx, int dy)
{
	float sensitivity = 0.25f;

	float hangle = (float)dx * sensitivity;
	float vangle = (float)dy * sensitivity;

	Position -= Reference;

	Y = rotate(Y, vangle, X);
	Z = rotate(Z, vangle, X);

	if(Y.y < 0.0f)
	{
		Z = vec3(0.0f, Z.y > 0.0f ? 1.0f : -1.0f, 0.0f);
		Y = cross(Z, X);
	}

	X = rotate(X, hangle, vec3(0.0f, 1.0f, 0.0f));
	Y = rotate(Y, hangle, vec3(0.0f, 1.0f, 0.0f));
	Z = rotate(Z, hangle, vec3(0.0f, 1.0f, 0.0f));

	Position = Reference + Z * length(Position);

	CalculateViewMatrix();
}

void CCamera::OnMouseWheel(short zDelta)
{
	Position -= Reference;

	if(zDelta < 0 && length(Position) < 500.0f)
	{
		Position += Position * 0.1f;
	}

	if(zDelta > 0 && length(Position) > 0.05f)
	{
		Position -= Position * 0.1f;
	}

	Position += Reference;

	CalculateViewMatrix();
}

void CCamera::SetViewMatrixPointer(mat4x4 *View)
{
	this->View = View;

	CalculateViewMatrix();
}

CCamera Camera;

// ----------------------------------------------------------------------------------------------------------------------------

COpenGLRenderer::COpenGLRenderer()
{
	ShowAxisGrid = true;
	Stop = false;

	Camera.SetViewMatrixPointer(&View);
}

COpenGLRenderer::~COpenGLRenderer()
{
}

bool COpenGLRenderer::Init()
{
	/*if(gl_version < 21)
	{
		ErrorLog.Set("OpenGL 2.1 not supported!");
		return false;
	}*/

	bool Error = false;

	Error |= !Texture.LoadTexture2D("golddiag.jpg");

	if(gl_version >= 21)
	{
		Error |= !Shader.Load("glsl120shader.vs", "glsl120shader.fs");
	}

	if(Error)
	{
		return false;
	}

	TexCoords = new vec2[24];
	Normals = new vec3[24];
	Vertices = new vec3[24];

	TexCoords[0] = vec2(0.0f, 0.0f); Normals[0] = vec3( 0.0f, 0.0f, 1.0f); Vertices[0] = vec3(-0.5f, -0.5f,  0.5f);
	TexCoords[1] = vec2(1.0f, 0.0f); Normals[1] = vec3( 0.0f, 0.0f, 1.0f); Vertices[1] = vec3( 0.5f, -0.5f,  0.5f);
	TexCoords[2] = vec2(1.0f, 1.0f); Normals[2] = vec3( 0.0f, 0.0f, 1.0f); Vertices[2] = vec3( 0.5f,  0.5f,  0.5f);
	TexCoords[3] = vec2(0.0f, 1.0f); Normals[3] = vec3( 0.0f, 0.0f, 1.0f); Vertices[3] = vec3(-0.5f,  0.5f,  0.5f);

	TexCoords[4] = vec2(0.0f, 0.0f); Normals[4] = vec3( 0.0f, 0.0f, -1.0f); Vertices[4] = vec3( 0.5f, -0.5f, -0.5f);
	TexCoords[5] = vec2(1.0f, 0.0f); Normals[5] = vec3( 0.0f, 0.0f, -1.0f); Vertices[5] = vec3(-0.5f, -0.5f, -0.5f);
	TexCoords[6] = vec2(1.0f, 1.0f); Normals[6] = vec3( 0.0f, 0.0f, -1.0f); Vertices[6] = vec3(-0.5f,  0.5f, -0.5f);
	TexCoords[7] = vec2(0.0f, 1.0f); Normals[7] = vec3( 0.0f, 0.0f, -1.0f); Vertices[7] = vec3( 0.5f,  0.5f, -0.5f);

	TexCoords[8] = vec2(0.0f, 0.0f); Normals[8] = vec3(1.0f, 0.0f, 0.0f); Vertices[8] = vec3( 0.5f, -0.5f,  0.5f);
	TexCoords[9] = vec2(1.0f, 0.0f); Normals[9] = vec3(1.0f, 0.0f, 0.0f); Vertices[9] = vec3( 0.5f, -0.5f, -0.5f);
	TexCoords[10] = vec2(1.0f, 1.0f); Normals[10] = vec3(1.0f, 0.0f, 0.0f); Vertices[10] = vec3( 0.5f,  0.5f, -0.5f);
	TexCoords[11] = vec2(0.0f, 1.0f); Normals[11] = vec3(1.0f, 0.0f, 0.0f); Vertices[11] = vec3( 0.5f,  0.5f,  0.5f);

	TexCoords[12] = vec2(0.0f, 0.0f); Normals[12] = vec3(-1.0f,  0.0f,  0.0f); Vertices[12] = vec3(-0.5f, -0.5f, -0.5f);
	TexCoords[13] = vec2(1.0f, 0.0f); Normals[13] = vec3(-1.0f,  0.0f,  0.0f); Vertices[13] = vec3(-0.5f, -0.5f,  0.5f);
	TexCoords[14] = vec2(1.0f, 1.0f); Normals[14] = vec3(-1.0f,  0.0f,  0.0f); Vertices[14] = vec3(-0.5f,  0.5f,  0.5f);
	TexCoords[15] = vec2(0.0f, 1.0f); Normals[15] = vec3(-1.0f,  0.0f,  0.0f); Vertices[15] = vec3(-0.5f,  0.5f, -0.5f);

	TexCoords[16] = vec2(0.0f, 0.0f); Normals[16] = vec3( 0.0f,  1.0f,  0.0f); Vertices[16] = vec3(-0.5f,  0.5f,  0.5f);
	TexCoords[17] = vec2(1.0f, 0.0f); Normals[17] = vec3( 0.0f,  1.0f,  0.0f); Vertices[17] = vec3( 0.5f,  0.5f,  0.5f);
	TexCoords[18] = vec2(1.0f, 1.0f); Normals[18] = vec3( 0.0f,  1.0f,  0.0f); Vertices[18] = vec3( 0.5f,  0.5f, -0.5f);
	TexCoords[19] = vec2(0.0f, 1.0f); Normals[19] = vec3( 0.0f,  1.0f,  0.0f); Vertices[19] = vec3(-0.5f,  0.5f, -0.5f);

	TexCoords[20] = vec2(0.0f, 0.0f); Normals[20] = vec3( 0.0f,  -1.0f,  0.0f); Vertices[20] = vec3(-0.5f, -0.5f, -0.5f);
	TexCoords[21] = vec2(1.0f, 0.0f); Normals[21] = vec3( 0.0f,  -1.0f,  0.0f); Vertices[21] = vec3( 0.5f, -0.5f, -0.5f);
	TexCoords[22] = vec2(1.0f, 1.0f); Normals[22] = vec3( 0.0f,  -1.0f,  0.0f); Vertices[22] = vec3( 0.5f, -0.5f,  0.5f);
	TexCoords[23] = vec2(0.0f, 1.0f); Normals[23] = vec3( 0.0f,  -1.0f,  0.0f); Vertices[23] = vec3(-0.5f, -0.5f,  0.5f);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	Camera.LookAt(vec3(0.0f, 0.0f, 0.0f), vec3(1.75f, 1.75f, 5.0f));

	// DisplayInfo("Information text ...");

	return true;
}

void COpenGLRenderer::Render(float FrameTime)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf((GLfloat*)&View);

	if(ShowAxisGrid)
	{
		glLineWidth(2.0f);

		glBegin(GL_LINES);

		glColor4f(1.0f, 0.0f, 0.0f, 1.0f);

		glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(1.0f, 0.0f, 0.0f);
		glVertex3f(1.0f, 0.1f, 0.0f); glVertex3f(1.1f, -0.1f, 0.0f);
		glVertex3f(1.1f, 0.1f, 0.0f); glVertex3f(1.0f, -0.1f, 0.0f);

		glColor4f(0.0f, 1.0f, 0.0f, 1.0f);

		glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, 1.0f, 0.0f);
		glVertex3f(-0.05f, 1.25f, 0.0f); glVertex3f(0.0f, 1.15f, 0.0f);
		glVertex3f(0.05f,1.25f, 0.0f); glVertex3f(0.0f, 1.15f, 0.0f);
		glVertex3f(0.0f,1.15f, 0.0f); glVertex3f(0.0f, 1.05f, 0.0f);

		glColor4f(0.0f, 0.0f, 1.0f, 1.0f);

		glVertex3f(0.0f,0.0f,0.0f); glVertex3f(0.0f, 0.0f, 1.0f);
		glVertex3f(-0.05f,0.1f,1.05f); glVertex3f(0.05f, 0.1f, 1.05f);
		glVertex3f(0.05f,0.1f,1.05f); glVertex3f(-0.05f, -0.1f, 1.05f);
		glVertex3f(-0.05f,-0.1f,1.05f); glVertex3f(0.05f, -0.1f, 1.05f);

		glEnd();

		glLineWidth(1.0f);

		glColor3f(1.0f, 1.0f, 1.0f);

		glBegin(GL_LINES);

		float d = 50.0f;

		for(float i = -d; i <= d; i += 1.0f)
		{
			glVertex3f(i, 0.0f, -d);
			glVertex3f(i, 0.0f, d);
			glVertex3f(-d, 0.0f, i);
			glVertex3f(d, 0.0f, i);
		}

		glEnd();
	}

	glMultMatrixf((GLfloat*)&Model);

	if(!Stop)
	{
		static float a = 0.0f;

		Model = rotate(mat4x4(), a, vec3(0.0f, 1.0f, 0.0f)) * rotate(mat4x4(), a, vec3(1.0f, 0.0f, 0.0f));

		a += 11.25f * FrameTime;
	}

	glEnable(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, Texture);

	if(gl_version >= 21)
	{
		glUseProgram(Shader);
	}

	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, TexCoords);

	glEnableClientState(GL_NORMAL_ARRAY);
	glNormalPointer(GL_FLOAT, 0, Normals);

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, Vertices);

	glDrawArrays(GL_QUADS, 0, 24);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	if(gl_version >= 21)
	{
		glUseProgram(0);
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	glDisable(GL_TEXTURE_2D);
}

void COpenGLRenderer::Resize(int Width, int Height)
{
	this->Width = Width;
	this->Height = Height;

	glViewport(0, 0, Width, Height);

	Projection = perspective(45.0f, (float)Width / (Height > 0 ? (float)Height : 1.0f), 0.125f, 512.0f);

	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf((GLfloat*)&Projection);
}

void COpenGLRenderer::Destroy()
{
	Texture.Delete();
	
	if(gl_version >= 21)
	{
		Shader.Delete();
	}

	delete [] TexCoords;
	delete [] Normals;
	delete [] Vertices;
}

COpenGLRenderer OpenGLRenderer;

// ----------------------------------------------------------------------------------------------------------------------------

CWnd::CWnd()
{
	char *moduledirectory = new char[256];
	GetModuleFileName(GetModuleHandle(NULL), moduledirectory, 256);
	*(strrchr(moduledirectory, '//') + 1) = 0;
	ModuleDirectory = moduledirectory;
	delete [] moduledirectory;

	DeFullScreened = false;
}

CWnd::~CWnd()
{
}

bool CWnd::Create(HINSTANCE hInstance, char *WindowName, int Width, int Height, bool FullScreen, int Samples, bool CreateForwardCompatibleContext, bool DisableVerticalSynchronization)
{
	WNDCLASSEX WndClassEx;

	memset(&WndClassEx, 0, sizeof(WNDCLASSEX));

	WndClassEx.cbSize = sizeof(WNDCLASSEX);
	WndClassEx.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	WndClassEx.lpfnWndProc = WndProc;
	WndClassEx.hInstance = hInstance;
	WndClassEx.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	WndClassEx.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	WndClassEx.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClassEx.lpszClassName = "Win32OpenGLWindowClass";

	if(!RegisterClassEx(&WndClassEx))
	{
		ErrorLog.Set("RegisterClassEx failed!");
		return false;
	}

	this->WindowName = WindowName;

    this->Width = Width;
    this->Height = Height;

	DWORD Style = (FullScreen ? WS_POPUP : WS_OVERLAPPEDWINDOW) | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

	if((hWnd = CreateWindowEx(WS_EX_APPWINDOW, WndClassEx.lpszClassName, WindowName, Style, 0, 0, Width, Height, NULL, NULL, hInstance, NULL)) == NULL)
	{
		ErrorLog.Set("CreateWindowEx failed!");
		return false;
	}

	this->FullScreen = FullScreen;

	if(FullScreen)
	{
		memset(&DevMode, 0, sizeof(DEVMODE));

		DevMode.dmSize = sizeof(DEVMODE);
		DevMode.dmPelsWidth = Width;
		DevMode.dmPelsHeight = Height;
		DevMode.dmBitsPerPel = 32;
		DevMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;

		this->FullScreen = ChangeDisplaySettings(&DevMode, CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL;
	}

	if((hDC = GetDC(hWnd)) == NULL)
	{
		ErrorLog.Set("GetDC failed!");
		return false;
	}

	PIXELFORMATDESCRIPTOR pfd;

	memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));

	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 24;
	pfd.iLayerType = PFD_MAIN_PLANE;

	int PixelFormat;

	if((PixelFormat = ChoosePixelFormat(hDC, &pfd)) == 0)
	{
		ErrorLog.Set("ChoosePixelFormat failed!");
		return false;
	}

	static int MSAAPixelFormat = 0;

	if(SetPixelFormat(hDC, MSAAPixelFormat == 0 ? PixelFormat : MSAAPixelFormat, &pfd) == FALSE)
	{
		ErrorLog.Set("SetPixelFormat failed!");
		return false;
	}

	if((hGLRC = wglCreateContext(hDC)) == NULL)
	{
		ErrorLog.Set("wglCreateContext failed!");
		return false;
	}

	if(wglMakeCurrent(hDC, hGLRC) == FALSE)
	{
		ErrorLog.Set("wglMakeCurrent failed!");
		return false;
	}

	if(glewInit() != GLEW_OK)
	{
		ErrorLog.Set("glewInit failed!");
		return false;
	}

	if(MSAAPixelFormat == 0 && Samples > 0)
	{
		if(GLEW_ARB_multisample && WGLEW_ARB_pixel_format)
		{
			while(Samples > 0)
			{
				UINT NumFormats = 0;

				int iAttributes[] =
				{
					WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
					WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
					WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
					WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
					WGL_COLOR_BITS_ARB, 32,
					WGL_DEPTH_BITS_ARB, 24,
					WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
					WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
					WGL_SAMPLES_ARB, Samples,
					0
				};

				if(wglChoosePixelFormatARB(hDC, iAttributes, NULL, 1, &MSAAPixelFormat, &NumFormats) == TRUE && NumFormats > 0) break;
				
				Samples--;
			}

			wglDeleteContext(hGLRC);

			DestroyWindow(hWnd);

			UnregisterClass(WndClassEx.lpszClassName, hInstance);

			return Create(hInstance, WindowName, Width, Height, FullScreen, Samples, CreateForwardCompatibleContext, DisableVerticalSynchronization);
		}
		else
		{
			Samples = 0;
		}
	}

	this->Samples = Samples;

	int major, minor;

	sscanf_s((char*)glGetString(GL_VERSION), "%d.%d", &major, &minor);

	gl_version = major * 10 + minor;

	if(CreateForwardCompatibleContext && gl_version >= 30 && WGLEW_ARB_create_context)
	{
		wglDeleteContext(hGLRC);

		int GLFCRCAttribs[] =
		{
			WGL_CONTEXT_MAJOR_VERSION_ARB, major,
			WGL_CONTEXT_MINOR_VERSION_ARB, minor,
			WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
			0
		};

		if((hGLRC = wglCreateContextAttribsARB(hDC, 0, GLFCRCAttribs)) == NULL)
		{
			ErrorLog.Set("wglCreateContextAttribsARB failed!");
			return false;
		}

		if(wglMakeCurrent(hDC, hGLRC) == FALSE)
		{
			ErrorLog.Set("wglMakeCurrent failed!");
			return false;
		}

		wgl_context_forward_compatible = true;
	}
	else
	{
		wgl_context_forward_compatible = false;
	}

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gl_max_texture_size);

	if(GLEW_EXT_texture_filter_anisotropic)
	{
		glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_max_texture_max_anisotropy_ext);
	}

	if(DisableVerticalSynchronization  && WGLEW_EXT_swap_control)
	{
		wglSwapIntervalEXT(0);
	}

	return OpenGLRenderer.Init();
}

void CWnd::Show(bool MouseGameMode, bool Maximized)
{
	this->MouseGameMode = MouseGameMode;

	if(!FullScreen)
	{
		RECT dRect, wRect, cRect;

		GetWindowRect(GetDesktopWindow(), &dRect);
		GetWindowRect(hWnd, &wRect);
		GetClientRect(hWnd, &cRect);

		wRect.right += Width - cRect.right;
		wRect.bottom += Height - cRect.bottom;
		
		wRect.right -= wRect.left;
		wRect.bottom -= wRect.top;

		wRect.left = dRect.right / 2 - wRect.right / 2;
		wRect.top = dRect.bottom / 2 - wRect.bottom / 2;

		MoveWindow(hWnd, wRect.left, wRect.top, wRect.right, wRect.bottom, FALSE);
	}
	else
	{
		this->MouseGameMode = true;
	}

	OpenGLRenderer.Resize(Width, Height);

	StartFPSCounter();

	ShowWindow(hWnd, (!FullScreen && Maximized) ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);

	SetForegroundWindow(hWnd);

	KeyBoardFocus = MouseFocus = true;

	SetCurAccToMouseGameMode();
}

void CWnd::MsgLoop()
{
	MSG Msg;

	while(GetMessage(&Msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}
}

void CWnd::Destroy()
{
	OpenGLRenderer.Destroy();

	wglDeleteContext(hGLRC);

	DestroyWindow(hWnd);
}

void CWnd::GetCurPos(int *cx, int *cy)
{
	POINT Point;

	GetCursorPos(&Point);

	ScreenToClient(hWnd, &Point);

	*cx = Point.x;
	*cy = Point.y;
}

void CWnd::SetCurPos(int cx, int cy)
{
	POINT Point;

	Point.x = cx;
	Point.y = cy;

	ClientToScreen(hWnd, &Point);

	SetCursorPos(Point.x, Point.y);
}

void CWnd::SetCurAccToMouseGameMode()
{
	if(MouseGameMode)
	{
		SetCurPos(WidthD2, HeightD2);
		while(ShowCursor(FALSE) >= 0);
	}
	else
	{
		while(ShowCursor(TRUE) < 0);
	}
}

void CWnd::SetMouseFocus()
{
	SetCurAccToMouseGameMode();
	MouseFocus = true;
}

void CWnd::StartFPSCounter()
{
	Start = Begin = GetTickCount();
}

void CWnd::OnKeyDown(UINT nChar)
{
	switch(nChar)
	{
		case VK_ESCAPE:
			PostQuitMessage(0);
			break;

		case VK_F1:
			OpenGLRenderer.ShowAxisGrid = !OpenGLRenderer.ShowAxisGrid;
			break;

		case VK_F2:
			if(!FullScreen && MouseFocus)
			{
				MouseGameMode = !MouseGameMode;
				SetCurAccToMouseGameMode();
			}
			break;

		case VK_F3:
			if(!FullScreen)
			{
				WINDOWPLACEMENT WndPlcm;
				WndPlcm.length = sizeof(WINDOWPLACEMENT);
				GetWindowPlacement(hWnd, &WndPlcm);
				if(WndPlcm.showCmd == SW_SHOWNORMAL) ShowWindow(hWnd, SW_SHOWMAXIMIZED);
				if(WndPlcm.showCmd == SW_SHOWMAXIMIZED) ShowWindow(hWnd, SW_SHOWNORMAL);
			}
			break;

		case VK_SPACE:
			OpenGLRenderer.Stop = !OpenGLRenderer.Stop;
			break;
	}
}

void CWnd::OnKillFocus()
{
	if(MouseFocus && MouseGameMode)
	{
		while(ShowCursor(TRUE) < 0);
	}

	KeyBoardFocus = MouseFocus = false;

	if(FullScreen)
	{
		ShowWindow(hWnd, SW_SHOWMINIMIZED);
		ChangeDisplaySettings(NULL, 0);
		DeFullScreened = true;
	}
}

void CWnd::OnLButtonDown(int cx, int cy)
{
	SetMouseFocus();
}

void CWnd::OnMouseMove(int cx, int cy)
{
	if(MouseGameMode && MouseFocus)
	{
		if(cx != WidthD2 || cy != HeightD2)
		{
			Camera.OnMouseMove(WidthD2 - cx, HeightD2 - cy);
			SetCurPos(WidthD2, HeightD2);
		}
	}
	else if(GetKeyState(VK_RBUTTON) & 0x80)
	{
		Camera.OnMouseMove(LastCurPos.x - cx, LastCurPos.y - cy);

		LastCurPos.x = cx;
		LastCurPos.y = cy;
	}
}

void CWnd::OnMouseWheel(short zDelta)
{
	Camera.OnMouseWheel(zDelta);
}

void CWnd::OnPaint()
{
	PAINTSTRUCT ps;

	BeginPaint(hWnd, &ps);

	static int FPS = 0;

	DWORD End = GetTickCount();

	float FrameTime = (End - Begin) * 0.001f;
	Begin = End;

	if(End - Start > 1000)
	{
		CString Text = WindowName;

		Text.Append(" - %dx%d", Width, Height);
		Text.Append(", ATF %dx", gl_max_texture_max_anisotropy_ext);
		Text.Append(", MSAA %dx", Samples);
		Text.Append(", FPS: %d", FPS);
		/*Text.Append(" - OpenGL %d.%d", gl_version / 10, gl_version % 10);
		if(gl_version >= 30) if(wgl_context_forward_compatible) Text.Append(" Forward compatible"); else Text.Append(" Compatibility profile");*/
		Text.Append(" - %s", (char*)glGetString(GL_RENDERER));

		SetWindowText(hWnd, Text);

		FPS = 0;
		Start = End;
	}
	else
	{
		FPS++;
	}

	if(KeyBoardFocus)
	{
		BYTE Keys = 0x00;

		if(GetKeyState('W') & 0x80) Keys |= 0x01;
		if(GetKeyState('S') & 0x80) Keys |= 0x02;
		if(GetKeyState('A') & 0x80) Keys |= 0x04;
		if(GetKeyState('D') & 0x80) Keys |= 0x08;
		if(GetKeyState('R') & 0x80) Keys |= 0x10;
		if(GetKeyState('F') & 0x80) Keys |= 0x20;

		if(GetKeyState(VK_SHIFT) & 0x80) Keys |= 0x40;

		if(Keys & 0x3F)
		{
			vec3 Movement = Camera.OnKeys(Keys, FrameTime);
			Camera.Move(Movement);
		}
	}

	OpenGLRenderer.Render(FrameTime);

	SwapBuffers(hDC);

	EndPaint(hWnd, &ps);

	InvalidateRect(hWnd, NULL, FALSE);
}

void CWnd::OnRButtonDown(int cx, int cy)
{
	SetMouseFocus();

	if(!MouseGameMode)
	{
		LastCurPos.x = cx;
		LastCurPos.y = cy;
	}
}

void CWnd::OnSetFocus()
{
	KeyBoardFocus = true;

	if(DeFullScreened)
	{
		ChangeDisplaySettings(&DevMode, CDS_FULLSCREEN);
		MoveWindow(hWnd, 0, 0, DevMode.dmPelsWidth, DevMode.dmPelsHeight, FALSE);
		DeFullScreened = false;
	}

	int cx, cy;

	GetCurPos(&cx, &cy);

	if(cx >= 0 && cx < Width && cy >= 0 && cy < Height)
	{
		SetMouseFocus();
	}
}

void CWnd::OnSize(int sx, int sy)
{
	if(Width == 0 && Height == 0)
	{
		StartFPSCounter();
	}

	Width = sx;
	Height = sy;

	WidthD2 = Width / 2;
	HeightD2 = Height / 2;

	OpenGLRenderer.Resize(Width, Height);
}

CWnd Wnd;

// ----------------------------------------------------------------------------------------------------------------------------

LRESULT CALLBACK WndProc(HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uiMsg)
	{
		case WM_CLOSE:
			PostQuitMessage(0);
			break;

		case WM_KEYDOWN:
			Wnd.OnKeyDown((UINT)wParam);
			break;

		case WM_KILLFOCUS:
			Wnd.OnKillFocus();
			break;

		case WM_LBUTTONDOWN:
			Wnd.OnLButtonDown(LOWORD(lParam), HIWORD(lParam));
			break;

		case WM_MOUSEMOVE:
			Wnd.OnMouseMove(LOWORD(lParam), HIWORD(lParam));
			break;

		case 0x020A: // WM_MOUSWHEEL
			Wnd.OnMouseWheel(HIWORD(wParam));
			break;

		case WM_PAINT:
			Wnd.OnPaint();
			break;

		case WM_RBUTTONDOWN:
			Wnd.OnRButtonDown(LOWORD(lParam), HIWORD(lParam));
			break;

		case WM_SETFOCUS:
			Wnd.OnSetFocus();
			break;

		case WM_SIZE:
			Wnd.OnSize(LOWORD(lParam), HIWORD(lParam));
			break;

		default:
			return DefWindowProc(hWnd, uiMsg, wParam, lParam);
	}

	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR sCmdLine, int iShow)
{
	if(Wnd.Create(hInstance, "Win32, OpenGL, GLEW, FreeImage, GLM", 800, 600, DisplayQuestion("Would you like to run in fullscreen mode?")))
	{
		Wnd.Show();
		Wnd.MsgLoop();
	}
	else
	{
		DisplayError(ErrorLog);
	}

	Wnd.Destroy();

	return 0;
}
