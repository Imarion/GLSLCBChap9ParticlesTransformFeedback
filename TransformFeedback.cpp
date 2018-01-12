#include "TransformFeedback.h"

#include <QtGlobal>

#include <QDebug>
#include <QFile>
#include <QImage>
#include <QTime>

#include <QVector2D>
#include <QVector3D>
#include <QMatrix4x4>

#include <../glm/glm.hpp>

#include <cmath>
#include <cstring>

MyWindow::~MyWindow()
{
    if (mProgram != 0) delete mProgram;
}

MyWindow::MyWindow()
    : mProgram(0), currentTimeMs(0), currentTimeS(0), tPrev(0), drawBuf(1), angle(M_PI/2.0f)
{
    setSurfaceType(QWindow::OpenGLSurface);
    setFlags(Qt::Window | Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setMajorVersion(4);
    format.setMinorVersion(3);
    format.setSamples(4);
    format.setProfile(QSurfaceFormat::CoreProfile);
    setFormat(format);
    create();

    resize(800, 600);

    mContext = new QOpenGLContext(this);
    mContext->setFormat(format);
    mContext->create();

    mContext->makeCurrent( this );

    mFuncs = mContext->versionFunctions<QOpenGLFunctions_4_3_Core>();
    if ( !mFuncs )
    {
        qWarning( "Could not obtain OpenGL versions object" );
        exit( 1 );
    }
    if (mFuncs->initializeOpenGLFunctions() == GL_FALSE)
    {
        qWarning( "Could not initialize core open GL functions" );
        exit( 1 );
    }

    initializeOpenGLFunctions();

    QTimer *repaintTimer = new QTimer(this);
    connect(repaintTimer, &QTimer::timeout, this, &MyWindow::render);
    repaintTimer->start(1000/60);

    QTimer *elapsedTimer = new QTimer(this);
    connect(elapsedTimer, &QTimer::timeout, this, &MyWindow::modCurTime);
    elapsedTimer->start(1);       
}

void MyWindow::modCurTime()
{
    currentTimeMs++;
    currentTimeS=currentTimeMs/1000.0f;
}

void MyWindow::initialize()
{
    CreateVertexBuffer();
    initShaders();
    initMatrices();

    PrepareTexture(GL_TEXTURE0, GL_TEXTURE_2D, "../Media/bluewater.png", false);

    renderSub = mFuncs->glGetSubroutineIndex(mProgram->programId(), GL_VERTEX_SHADER, "render");
    updateSub = mFuncs->glGetSubroutineIndex(mProgram->programId(), GL_VERTEX_SHADER, "update");

    glFrontFace(GL_CCW);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    mFuncs->glPointSize(10.0f);
}

void MyWindow::CreateVertexBuffer()
{
    nParticles = 8000;

    // Create and populate the buffer objects
    unsigned int posBuf[2], velBuf[2], startTimeBuf[2], initVelBuf;

    glGenBuffers(2, posBuf);
    glGenBuffers(2, velBuf);
    glGenBuffers(2, startTimeBuf);
    glGenBuffers(1, &initVelBuf);

    int size = 3* nParticles * sizeof(float);

    glBindBuffer(GL_ARRAY_BUFFER, posBuf[0]);
    glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_DYNAMIC_COPY);
    glBindBuffer(GL_ARRAY_BUFFER, posBuf[1]);
    glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_DYNAMIC_COPY);

    glBindBuffer(GL_ARRAY_BUFFER, velBuf[0]);
    glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_DYNAMIC_COPY);
    glBindBuffer(GL_ARRAY_BUFFER, velBuf[1]);
    glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_DYNAMIC_COPY);

    glBindBuffer(GL_ARRAY_BUFFER, initVelBuf);
    glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, startTimeBuf[0]);
    glBufferData(GL_ARRAY_BUFFER, nParticles * sizeof(float), NULL, GL_DYNAMIC_COPY);
    glBindBuffer(GL_ARRAY_BUFFER, startTimeBuf[1]);
    glBufferData(GL_ARRAY_BUFFER, nParticles * sizeof(float), NULL, GL_DYNAMIC_COPY);

    // Fill the first position buffer with zeroes
    GLfloat *data = new GLfloat[nParticles * 3];
    for(int i = 0; i < nParticles * 3; i++ ) data[i] = 0.0f;
    glBindBuffer(GL_ARRAY_BUFFER, posBuf[0]);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);

    // Velocity data
    QVector3D v;
    float     velocity, theta, phi;

    for (unsigned int i = 0; i < nParticles; i++ )
    {
        theta = glm::mix(0.0f, (float)M_PI / 6.0f, randFloat());
        phi   = glm::mix(0.0f, (float)M_PI * 2.0f, randFloat());

        v.setX(sinf(theta) * cosf(phi));
        v.setY(cosf(theta));
        v.setZ(sinf(theta) * sinf(phi));

        velocity = glm::mix(1.25f, 1.5f, randFloat());
        v = v.normalized() * velocity;

        data[3*i]   = v.x();
        data[3*i+1] = v.y();
        data[3*i+2] = v.z();
    }
    glBindBuffer(GL_ARRAY_BUFFER, velBuf[0]);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
    glBindBuffer(GL_ARRAY_BUFFER, initVelBuf);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
    delete [] data;

    // Start time data
    data = new GLfloat[nParticles];
    float time = 0.0f;
    float rate = 0.00075f;
    for( unsigned int i = 0; i < nParticles; i++ )
    {
        data[i] = time;
        time   += rate;
    }
    glBindBuffer(GL_ARRAY_BUFFER, startTimeBuf[0]);
    glBufferSubData(GL_ARRAY_BUFFER, 0, nParticles * sizeof(float), data);
    delete [] data;

    // Setup the VAOs
    mFuncs->glGenVertexArrays(2, mVAOParticles);

    // particle array A
    mFuncs->glBindVertexArray(mVAOParticles[0]);

    mFuncs->glBindVertexBuffer(0, posBuf[0], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(0, 0);

    mFuncs->glBindVertexBuffer(1, velBuf[0], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(1, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(1, 1);

    mFuncs->glBindVertexBuffer(2, startTimeBuf[0], 0, sizeof(GLfloat));
    mFuncs->glVertexAttribFormat(2, 1, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(2, 2);

    mFuncs->glBindVertexBuffer(3, initVelBuf, 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(3, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(3, 3);

    // particle array B
    mFuncs->glBindVertexArray(mVAOParticles[1]);

    mFuncs->glBindVertexBuffer(0, posBuf[1], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(0, 0);

    mFuncs->glBindVertexBuffer(1, velBuf[1], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(1, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(1, 1);

    mFuncs->glBindVertexBuffer(2, startTimeBuf[1], 0, sizeof(GLfloat));
    mFuncs->glVertexAttribFormat(2, 1, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(2, 2);

    mFuncs->glBindVertexBuffer(3, initVelBuf, 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(3, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(3, 3);

    mFuncs->glBindVertexArray(0);

    // Setup the feedback objects
    mFuncs->glGenTransformFeedbacks(2, feedback);

    // Transform feedback 0
    mFuncs->glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, feedback[0]);
    mFuncs->glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, posBuf[0]);
    mFuncs->glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 1, velBuf[0]);
    mFuncs->glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 2, startTimeBuf[0]);

    // Transform feedback 1
    mFuncs->glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, feedback[1]);
    mFuncs->glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, posBuf[1]);
    mFuncs->glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 1, velBuf[1]);
    mFuncs->glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 2, startTimeBuf[1]);

    mFuncs->glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

    GLint value;
    glGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_BUFFERS, &value);
    qDebug() << "MAX_TRANSFORM_FEEDBACK_BUFFERS = " << value << endl;
}

void MyWindow::initMatrices()
{
    ViewMatrix.lookAt(QVector3D(3.0f * cos(angle), 1.5f, 3.0f * sin(angle)), QVector3D(0.0f, 1.5f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f));
}

void MyWindow::resizeEvent(QResizeEvent *)
{
    mUpdateSize = true;

    ProjectionMatrix.setToIdentity();
    ProjectionMatrix.perspective(60.0f, (float)this->width()/(float)this->height(), 0.3f, 100.0f);
}

void MyWindow::render()
{
    if(!isVisible() || !isExposed())
        return;

    if (!mContext->makeCurrent(this))
        return;

    static bool initialized = false;
    if (!initialized) {
        initialize();
        initialized = true;
    }

    if (mUpdateSize) {
        glViewport(0, 0, size().width(), size().height());
        mUpdateSize = false;
    }

    deltaT = currentTimeS - tPrev;
    if(tPrev == 0.0f) deltaT = 0.0f;
    tPrev = currentTimeS;
    angle += 0.25f * deltaT;
    if (angle > TwoPI) angle -= TwoPI;

    static float EvolvingVal = 0.0f;

    if (animate == true) EvolvingVal += 0.01f;

    glClearColor(0.1f, 0.1f, 0.1f,1.0f);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);

    mProgram->bind();
    {        
        // Update pass
        mFuncs->glUniformSubroutinesuiv(GL_VERTEX_SHADER, 1, &updateSub);

        mProgram->setUniformValue("Time", (float)currentTimeS);
        mProgram->setUniformValue("H",    (float)deltaT);

        glEnable(GL_RASTERIZER_DISCARD);
        mFuncs->glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, feedback[drawBuf]);

        mFuncs->glBeginTransformFeedback(GL_POINTS);
            mFuncs->glBindVertexArray(mVAOParticles[1-drawBuf]);
            glDrawArrays(GL_POINTS, 0, nParticles);
        mFuncs->glEndTransformFeedback();

        glDisable(GL_RASTERIZER_DISCARD);


        // Render pass
        mFuncs->glUniformSubroutinesuiv(GL_VERTEX_SHADER, 1, &renderSub);

        glClear( GL_COLOR_BUFFER_BIT );

        QMatrix4x4 mv1 = ViewMatrix;
        mProgram->setUniformValue("MVP", ProjectionMatrix * mv1);

        mProgram->setUniformValue("ParticleTex",      0);

        mFuncs->glBindVertexArray(mVAOParticles[drawBuf]);
        mFuncs->glDrawTransformFeedback(GL_POINTS, feedback[drawBuf]);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
        glDisableVertexAttribArray(3);

        // Swap buffers
        drawBuf = 1 - drawBuf;
    }
    mProgram->release();

    mContext->swapBuffers(this);
}

void MyWindow::initShaders()
{
    QOpenGLShader vShader(QOpenGLShader::Vertex);
    QOpenGLShader fShader(QOpenGLShader::Fragment);    
    QFile         shaderFile;
    QByteArray    shaderSource;

    //Simple ADS
    shaderFile.setFileName(":/vshader.txt");
    shaderFile.open(QIODevice::ReadOnly);
    shaderSource = shaderFile.readAll();
    shaderFile.close();
    qDebug() << "vertex compile: " << vShader.compileSourceCode(shaderSource);

    shaderFile.setFileName(":/fshader.txt");
    shaderFile.open(QIODevice::ReadOnly);
    shaderSource = shaderFile.readAll();
    shaderFile.close();
    qDebug() << "frag   compile: " << fShader.compileSourceCode(shaderSource);

    mProgram = new (QOpenGLShaderProgram);
    mProgram->addShader(&vShader);
    mProgram->addShader(&fShader);

    // Setup the transform feedback (must be done before linking the program)
    const char *outputNames[] = { "Position", "Velocity", "StartTime" };
    mFuncs->glTransformFeedbackVaryings(mProgram->programId(), 3, outputNames, GL_SEPARATE_ATTRIBS);

    qDebug() << "shader link: " << mProgram->link();
}

void MyWindow::PrepareTexture(GLenum TextureUnit, GLenum TextureTarget, const QString& FileName, bool flip)
{
    QImage TexImg;

    if (!TexImg.load(FileName)) qDebug() << "Erreur chargement texture " << FileName;
    if (flip==true) TexImg=TexImg.mirrored();

    glActiveTexture(TextureUnit);
    GLuint TexObject;
    glGenTextures(1, &TexObject);
    glBindTexture(TextureTarget, TexObject);
    mFuncs->glTexStorage2D(TextureTarget, 1, GL_RGB8, TexImg.width(), TexImg.height());
    mFuncs->glTexSubImage2D(TextureTarget, 0, 0, 0, TexImg.width(), TexImg.height(), GL_BGRA, GL_UNSIGNED_BYTE, TexImg.bits());
    glTexParameteri(TextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(TextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void MyWindow::keyPressEvent(QKeyEvent *keyEvent)
{
    switch(keyEvent->key())
    {
        case Qt::Key_P:
            break;
        case Qt::Key_Up:
            break;
        case Qt::Key_Down:
            break;
        case Qt::Key_Left:
            break;
        case Qt::Key_Right:
            break;
        case Qt::Key_Delete:
            break;
        case Qt::Key_PageDown:
            break;
        case Qt::Key_Home:
            break;
        case Qt::Key_Z:
            break;
        case Qt::Key_Q:
            break;
        case Qt::Key_S:
            break;
        case Qt::Key_A:
            animate = !animate;
            break;
        case Qt::Key_W:
            break;
        case Qt::Key_E:
            break;
        default:
            break;
    }
}

float MyWindow::randFloat() {
    return ((float)rand() / RAND_MAX);
}

void MyWindow::printMatrix(const QMatrix4x4& mat)
{
    const float *locMat = mat.transposed().constData();

    for (int i=0; i<4; i++)
    {
        qDebug() << locMat[i*4] << " " << locMat[i*4+1] << " " << locMat[i*4+2] << " " << locMat[i*4+3];
    }
}
