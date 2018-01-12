#include <QWindow>
#include <QTimer>
#include <QString>
#include <QKeyEvent>

#include <QVector3D>
#include <QMatrix4x4>

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFunctions_4_3_Core>

#include <QOpenGLShaderProgram>

#define ToRadian(x) ((x) * M_PI / 180.0f)
#define ToDegree(x) ((x) * 180.0f / M_PI)
#define TwoPI (float)(2 * M_PI)

//class MyWindow : public QWindow, protected QOpenGLFunctions_3_3_Core
class MyWindow : public QWindow, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit MyWindow();
    ~MyWindow();
    virtual void keyPressEvent( QKeyEvent *keyEvent );    

private slots:
    void render();

private:    
    void initialize();
    void modCurTime();

    void initShaders();
    void CreateVertexBuffer();
    void GenerateTexture(float baseFreq, float persistence, int w, int h, bool periodic);
    void initMatrices();

    float randFloat();

    void  PrepareTexture(GLenum TextureUnit, GLenum TextureTarget, const QString& FileName, bool flip);

protected:
    void resizeEvent(QResizeEvent *);

private:
    QOpenGLContext *mContext;
    QOpenGLFunctions_4_3_Core *mFuncs;

    QOpenGLShaderProgram *mProgram;

    QTimer mRepaintTimer;
    double currentTimeMs;
    double currentTimeS, deltaT;
    bool   mUpdateSize;
    float  tPrev, angle;

    GLuint       mVAOParticles[2], feedback[2], mVBO, mIBO;
    GLuint       renderSub, updateSub;
    unsigned int drawBuf;

    QMatrix4x4   ViewMatrix, ProjectionMatrix;

    bool         animate    = false;
    unsigned int nParticles = 8000;

    //debug
    void printMatrix(const QMatrix4x4& mat);
};
