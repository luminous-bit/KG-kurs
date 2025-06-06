#include "Render.h"
#include "GUItextRectangle.h"
#include "MyShaders.h"
#include "ObjLoader.h"
#include "Texture.h"

#include <GL/gl.h>
#include <GL/glu.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <windows.h>

#include "debout.h"

// Внутренняя логика "движка"
#include "MyOGL.h"
extern OpenGL gl;
#include "Light.h"
Light light;
#include "Camera.h"
Camera camera;

bool texturing = true;
bool lightning = true;
bool alpha = false;
bool stop = false;
bool going_back = false;

inline void f_ermit(double t, double X[], double P1[], double P4[], double R1[], double R4[]) {
    X[0] = P1[0] * (2 * pow(t, 3) - 3 * pow(t, 2) + 1) + P4[0] * (-2 * pow(t, 3) + 3 * pow(t, 2)) + R1[0] * (pow(t, 3) - 2 * pow(t, 2) + t) + R4[0] * (pow(t, 3) - pow(t, 2));
    X[1] = P1[1] * (2 * pow(t, 3) - 3 * pow(t, 2) + 1) + P4[1] * (-2 * pow(t, 3) + 3 * pow(t, 2)) + R1[1] * (pow(t, 3) - 2 * pow(t, 2) + t) + R4[1] * (pow(t, 3) - pow(t, 2));
    X[2] = P1[2] * (2 * pow(t, 3) - 3 * pow(t, 2) + 1) + P4[2] * (-2 * pow(t, 3) + 3 * pow(t, 2)) + R1[2] * (pow(t, 3) - 2 * pow(t, 2) + t) + R4[2] * (pow(t, 3) - pow(t, 2));
}

// Переключение режимов освещения, текстурирования, альфа-наложения
void switchModes(OpenGL* sender, KeyEventArg arg)
{
    // Конвертируем код клавиши в букву
    auto key = LOWORD(MapVirtualKeyA(arg.key, MAPVK_VK_TO_CHAR));

    switch (key)
    {
    case 'L':
        lightning = !lightning;
        break;
    case 'T':
        texturing = !texturing;
        break;
    case 'A':
        alpha = !alpha;
        break;
    case 'S':
        stop = !stop;
        break;
    }
}

// Умножение матриц c[M1][N1] = a[M1][N1] * b[M2][N2]
template <typename T, int M1, int N1, int M2, int N2> void MatrixMultiply(const T* a, const T* b, T* c)
{
    for (int i = 0; i < M1; ++i)
    {
        for (int j = 0; j < N2; ++j)
        {
            c[i * N2 + j] = T(0);
            for (int k = 0; k < N1; ++k)
            {
                c[i * N2 + j] += a[i * N1 + k] * b[k * N2 + j];
            }
        }
    }
}

// Текстовый прямоугольник в верхнем правом углу.
// OGL не предоставляет возможности для хранения текста;
// внутри этого класса создается картинка с текстом (через GDI),
// в виде текстуры накладывается на прямоугольник и рисуется на экране.
// Это самый простой, но очень неэффективный способ написать что-либо на экране.
GuiTextRectangle text;

// ID для текстуры
GLuint texId;

ObjModel f, f_station, f_car;

Shader cassini_sh;
Shader phong_sh;
Shader vb_sh;
Shader simple_texture_sh;

Texture stankin_tex, vb_tex, monkey_tex, station_tex, car_tex;

// Выполняется один раз перед первым рендером
void initRender()
{
    // Настройка шейдеров
    cassini_sh.VshaderFileName = "shaders/v.vert";
    cassini_sh.FshaderFileName = "shaders/cassini.frag";
    cassini_sh.LoadShaderFromFile();
    cassini_sh.Compile();

    phong_sh.VshaderFileName = "shaders/v.vert";
    phong_sh.FshaderFileName = "shaders/light.frag";
    phong_sh.LoadShaderFromFile();
    phong_sh.Compile();

    vb_sh.VshaderFileName = "shaders/v.vert";
    vb_sh.FshaderFileName = "shaders/vb.frag";
    vb_sh.LoadShaderFromFile();
    vb_sh.Compile();

    simple_texture_sh.VshaderFileName = "shaders/v.vert";
    simple_texture_sh.FshaderFileName = "shaders/textureShader.frag";
    simple_texture_sh.LoadShaderFromFile();
    simple_texture_sh.Compile();

    stankin_tex.LoadTexture("textures/stankin.png");
    vb_tex.LoadTexture("textures/vb.png");
    station_tex.LoadTexture("textures/fstation.png");
    //car_tex.LoadTexture("textures/jeep.png");

    f_station.LoadModel("models//fstation.obj_m");
    f_car.LoadModel("models//jeep.obj_m");
    //==============НАСТРОЙКА ТЕКСТУР================
    // 4 байта на хранение пикселя
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    //================НАСТРОЙКА КАМЕРЫ======================
    camera.caclulateCameraPos();

    // привязываем камеру к событиям "движка"
    gl.WheelEvent.reaction(&camera, &Camera::Zoom);
    gl.MouseMovieEvent.reaction(&camera, &Camera::MouseMovie);
    gl.MouseLeaveEvent.reaction(&camera, &Camera::MouseLeave);
    gl.MouseLdownEvent.reaction(&camera, &Camera::MouseStartDrag);
    gl.MouseLupEvent.reaction(&camera, &Camera::MouseStopDrag);
    //==============НАСТРОЙКА СВЕТА===========================
    // Привязываем свет к событиям "движка"
    gl.MouseMovieEvent.reaction(&light, &Light::MoveLight);
    gl.KeyDownEvent.reaction(&light, &Light::StartDrug);
    gl.KeyUpEvent.reaction(&light, &Light::StopDrug);
    //========================================================
    //====================Прочее==============================
    gl.KeyDownEvent.reaction(switchModes);
    text.setSize(512, 180);
    //========================================================

    camera.setPosition(2, 1.5, 1.5);
}

float view_matrix[16];
double full_time = 0;
int location = 0;

double t_f = 0;

void Render(double delta_time)
{
    full_time += delta_time;

    // Настройка камеры и света
    if (gl.isKeyPressed('F')) // если нажата F - свет из камеры
    {
        light.SetPosition(camera.x(), camera.y(), camera.z());
    }
    camera.SetUpCamera();
    // Забираем матрицу MODELVIEW сразу после установки камеры,
    // так как в ней отсутствуют трансформации glRotate
    glGetFloatv(GL_MODELVIEW_MATRIX, view_matrix);

    light.SetUpLight();

    // Рисуем оси
    gl.DrawAxes();

    glBindTexture(GL_TEXTURE_2D, 0);

    // Включаем нормализацию нормалей
    // чтобы glScaled не влияли на них.

    glEnable(GL_NORMALIZE);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    // Переключаем режимы (см void switchModes(OpenGL *sender, KeyEventArg arg))
    if (lightning)
        glEnable(GL_LIGHTING);
    if (texturing)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0); // Сбрасываем текущую текстуру
    }

    if (alpha)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    //=============НАСТРОЙКА МАТЕРИАЛА==============

    // Настройка материала, все что рисуется ниже будет иметь этот материал.
    // Массивы с настройками материала
    float amb[] = {0.2, 0.2, 0.1, 1.};
    float dif[] = {0.4, 0.65, 0.5, 1.};
    float spec[] = {0.9, 0.8, 0.3, 1.};
    float sh = 0.2f * 256;

    // Фоновая
    glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
    // Дифузная
    glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
    // Зеркальная
    glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
    // Размер блика
    glMaterialf(GL_FRONT, GL_SHININESS, sh);

    // Сглаживание освещения
    glShadeModel(GL_SMOOTH); // закраска по Гуро
                             //(GL_SMOOTH - плоская закраска)

    //============ РИСОВАТЬ ТУТ ==============

    //Car
    glPushMatrix();
    Shader::DontUseShaders();
    /*glActiveTexture(GL_TEXTURE0);
    car_tex.Bind();*/
    glShadeModel(GL_SMOOTH);
    glTranslated(1.5, t_f, 0);
    glScaled(0.1, 0.1, 0.1);
    glRotated(180, 0, 1, 1);
    if (!stop)
    {
        if (going_back) t_f -= 0.001;
        else t_f += 0.001;
        if (t_f >= 3 || t_f <= -3) going_back = !going_back;
    }
    f_car.Draw();
    glPopMatrix();

    /*if (going_back) t_f -= 0.001;
    else t_f += 0.001;*/

    //Заправка
    glPushMatrix();
    Shader::DontUseShaders();
    glActiveTexture(GL_TEXTURE0);
    station_tex.Bind();
    glShadeModel(GL_SMOOTH);
    glTranslated(0, 0, 1.1);
    glScaled(0.1, 0.1, 0.1);
    glRotated(180, 0, 0, 1);
    f_station.Draw();
    glPopMatrix();

    glColor3d(0.3, 0.5, 7);
    glBegin(GL_LINES);
    glVertex3d(1.5, -3, 0);
    glVertex3d(1.5, 3, 0);
    glEnd();

    //===============================================

    // Сбрасываем все трансформации
    glLoadIdentity();
    camera.SetUpCamera();
    Shader::DontUseShaders();
    // Рисуем источник света
    light.DrawLightGizmo();

    //================Сообщение в верхнем левом углу=======================
    glActiveTexture(GL_TEXTURE0);
    // Переключаемся на матрицу проекции
    glMatrixMode(GL_PROJECTION);
    // Сохраняем текущую матрицу проекции с перспективным преобразованием
    glPushMatrix();
    // Загружаем единичную матрицу в матрицу проекции
    glLoadIdentity();

    // Устанавливаем матрицу параллельной проекции
    glOrtho(0, gl.getWidth() - 1, 0, gl.getHeight() - 1, 0, 1);

    // Переключаемся на матрицу MODELVIEW
    glMatrixMode(GL_MODELVIEW);
    // Сохраняем матрицу
    glPushMatrix();
    // Сбрасываем все трансформации и настройки камеры загрузкой единичной матрицы
    glLoadIdentity();

    // Нарисованное тут находится в 2D системе координат
    // Нижний левый угол окна - точка (0,0)
    // Верхний правый угол (ширина_окна - 1, высота_окна - 1)

    std::wstringstream ss;
    ss << std::fixed << std::setprecision(3) << "T - " << (texturing ? L"[вкл]выкл" : L"вкл[выкл]") << L" текстур\n"
       << "L - " << (lightning ? L"[вкл]выкл" : L"вкл[выкл]") << L" освещение\n"
       << "A - " << (alpha ? L"[вкл]выкл" : L"вкл[выкл]") << L" альфа-наложение\n"
       << L"S - остановить движение\n"
       << L"F - переместить свет в позицию камеры\n"
       << L"G - двигать свет по горизонтали\n"
       << L"G+ЛКМ - двигать свет по вертикали\n"
       << L"Координаты света: (" << std::setw(7) << light.x() << "," << std::setw(7) << light.y() << "," << std::setw(7)
       << light.z() << ")\n"
       << L"Координаты камеры: (" << std::setw(7) << camera.x() << "," << std::setw(7) << camera.y() << ","
       << std::setw(7) << camera.z() << ")\n"
       << L"Параметры камеры: R=" << std::setw(7) << camera.distance() << ", fi1=" << std::setw(7) << camera.fi1()
       << ", fi2=" << std::setw(7) << camera.fi2() << '\n'
       << L"delta_time: " << std::setprecision(5) << delta_time << '\n'
       << L"full_time: " << std::setprecision(2) << full_time << std::endl;

    text.setPosition(10, gl.getHeight() - 10 - 180);
    text.setText(ss.str().c_str());
    text.Draw();

    // Восстанавливаем матрицу проекции на перспективу, которую сохраняли ранее.
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}
