#include "mainwindow.h"
#include <QtWebKit>
#include <QStringList>
#include <QWebView>
#include <QDockWidget>
#include <QtDebug>
#include <QWebPage>
#include <QProcess>
#include <QTextEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QDateTime>
#include <QProgressBar>
#include <QTabWidget>
#include <QWebView>
#include <QLineEdit>

#include "qtermwidget.h"

#define BUFFERSIZE 2048
#define REGEXP_STRING "^\\[(\\d+)\\]"

#define CURRENT_TIME() QDateTime::currentDateTime().toString("dd/MM/yy hh:mm:ss")

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),m_web(new QWebView()),m_output(new QTextEdit()),
      m_process(new QProcess(this)),
      m_logfile(0),m_logstream(0),m_numberTerminal(0)
{
    // Graphic
    showFullScreen();

    setWindowTitle(tr("OpenGnSys Browser"));

    setCentralWidget(m_web);

    readEnvironmentValues();

    // Output
    m_output->setReadOnly(true);

    // Button Dock
    QDockWidget* dock=new QDockWidget();
    dock->setAllowedAreas(Qt::BottomDockWidgetArea);
    QWidget* dummy=new QWidget();
    dummy->setMaximumHeight(0);
    dock->setTitleBarWidget(dummy);

    // TabWidget
    m_tabs=new QTabWidget(dock);
    QPushButton *button=new QPushButton(tr("&Nueva Terminal"));
    button->setFocusPolicy(Qt::TabFocus);
    m_tabs->setCornerWidget(button);
    m_tabs->setFocusPolicy(Qt::NoFocus);

    m_tabs->addTab(m_output,tr("Salida"));
    slotCreateTerminal();

    // Las pestanyas al dock
    dock->setWidget(m_tabs);

    // Y el dock al mainwindow
    if(m_env.contains("boot") && m_env["boot"] == "admin")
      addDockWidget(Qt::BottomDockWidgetArea,dock);

    // Top Dock
    dock=new QDockWidget();
    dock->setAllowedAreas(Qt::TopDockWidgetArea);
    QWidget* dummy2=new QWidget();
    dummy2->setMaximumHeight(0);
    dock->setTitleBarWidget(dummy2);

    // WebBar
    m_webBar=new QLineEdit(dock);

    // WebBar al dock
    dock->setWidget(m_webBar);

    // dock al mainwindow
    if(m_env.contains("boot") && m_env["boot"] == "admin")
      addDockWidget(Qt::TopDockWidgetArea,dock);

    // Status bar
    QStatusBar* st=statusBar();
    st->setSizeGripEnabled(false);
    m_progressBar=new QProgressBar(this);
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(100);

    m_web->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);

    // Web signals
    connect(m_web,SIGNAL(linkClicked(const QUrl&)),this,
            SLOT(slotLinkHandle(const QUrl&)));
    connect(m_web,SIGNAL(loadStarted()),this,SLOT(slotWebLoadStarted()));
    connect(m_web,SIGNAL(loadFinished(bool)),this,SLOT(slotWebLoadFinished(bool)));
    connect(m_web,SIGNAL(loadProgress(int)),this,SLOT(slotWebLoadProgress(int)));
    connect(m_web,SIGNAL(urlChanged(const QUrl&)),this,
            SLOT(slotUrlChanged(const QUrl&)));

    // Process signals
    connect(m_process,SIGNAL(started()),this,SLOT(slotProcessStarted()));
    connect(m_process,SIGNAL(finished(int,QProcess::ExitStatus)),
            this,SLOT(slotProcessFinished(int,QProcess::ExitStatus)));

    connect(m_process,SIGNAL(error(QProcess::ProcessError)),
            this,SLOT(slotProcessError(QProcess::ProcessError)));

    connect(m_process,SIGNAL(readyReadStandardOutput()),this,SLOT(slotProcessOutput()));
    connect(m_process,SIGNAL(readyReadStandardError()),
            this,SLOT(slotProcessErrorOutput()));

    // Dock signals
    connect(button,SIGNAL(clicked()),this,SLOT(slotCreateTerminal()));
    connect(m_webBar,SIGNAL(returnPressed()),this,SLOT(slotWebBarReturnPressed()));

    if(m_env.contains("OGLOGFILE") && m_env["OGLOGFILE"]!="")
    {
        QFile* file=new QFile(m_env["OGLOGFILE"]);
        if(!file->open(QIODevice::WriteOnly | QIODevice::Text |
                    QIODevice::Append))
        {
            delete file;
            print(tr("El fichero de log no ha podido ser abierto: ")+m_env["OGLOGFILE"]+".");
        }
        else
        {
            m_logfile=file;
            m_logstream=new QTextStream(m_logfile);
        }
    }

    QStringList arguments=QCoreApplication::arguments();
    m_webBar->setText(arguments[1]);
    m_web->load(QUrl(arguments[1]));
}

MainWindow::~MainWindow()
{
    if(m_logfile)
    {
        m_logfile->close();
        delete m_logfile;
    }
    if(m_logstream)
        delete m_logstream;
}

void MainWindow::slotLinkHandle(const QUrl &url)
{
    // Si ya hay un proceso ejectuandose
    if(m_process->state()!=QProcess::NotRunning)
    {
      print(tr("Hay otro proceso en ejecuci�n. Por favor espere."));
      return;
    }
 
    QString urlString = url.toString();
    if(urlString.startsWith(COMMAND))
    {
        // Si es link de tipo COMMAND, ejecutar.
        executeCommand(urlString.remove(0,QString(COMMAND).length()));
    }
    else if(urlString.startsWith(COMMAND_WITH_CONFIRMATION))
    {
        // Si es link de tipo COMMAND_WITH_CONFIRMATION, pedir confirmaci�n de ejecutar.
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setWindowTitle(tr("AVISO"));
        msgBox.setTextFormat(Qt::RichText);
        msgBox.setText(tr("La siguiente acci&oacute;n puede modificar datos o tardar varios minutos. El equipo no podr&aacute; ser utilizado durante su ejecuci&oacute;n."));
        QPushButton *execButton = msgBox.addButton(tr("Ejecutar"), QMessageBox::ActionRole);
        msgBox.addButton(tr("Cancelar"), QMessageBox::RejectRole);
        msgBox.setDefaultButton(execButton);
        msgBox.exec();
        if (msgBox.clickedButton() == execButton)
        {
            executeCommand(urlString.remove(0,QString(COMMAND_WITH_CONFIRMATION).length()));
        }
    }
    else
    {
        // Si es otro link, cargar p�gina web.
        m_web->load(url);
    }
}

void MainWindow::slotWebLoadStarted()
{
    startProgressBar();
    m_progressBar->setFormat("%p% Cargando");
}

void MainWindow::slotWebLoadProgress(int progress)
{
    m_progressBar->setValue(progress);
}

void MainWindow::slotWebLoadFinished(bool ok)
{
    // If any error ocurred, show a pop up
    // Sometimes when the url hasn't got a dot, i.e /var/www/pageweb,
    // the return value is always true so we check the bytes received too
    if(ok == false)
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setWindowTitle(tr("AVISO"));
        msgBox.setTextFormat(Qt::RichText);
        msgBox.setText(tr("La p&aacute;gina no se puede cargar."));

        QPushButton *reloadButton = msgBox.addButton(tr("Recargar"), QMessageBox::ActionRole);
        msgBox.addButton(tr("Abortar"), QMessageBox::RejectRole);

        msgBox.exec();

        if (msgBox.clickedButton() == reloadButton)
        {
            m_web->reload();
        }
        else
        {
            close();
        }
    }
    else
    {
        finishProgressBar();
    }
}

void MainWindow::slotUrlChanged(const QUrl &url)
{
    m_webBar->setText(url.toString());
}

void MainWindow::slotProcessStarted()
{
    print(tr("Lanzado satisfactoriamente."));
    startProgressBar();
}

void MainWindow::slotProcessOutput()
{
    m_process->setReadChannel(QProcess::StandardOutput);
    char buf[BUFFERSIZE];
    while((m_process->readLine(buf,BUFFERSIZE) > 0))
    {
        QString s(buf);
        print(tr("Proc. stdout: ")+s);
        captureOutputForStatusBar(s);
    }
}

void MainWindow::slotProcessErrorOutput()
{
    m_process->setReadChannel(QProcess::StandardError);
    char buf[BUFFERSIZE];
    while((m_process->readLine(buf,BUFFERSIZE) > 0))
    {
        QString s(buf);
        m_output->insertPlainText(tr("Proc. stderr: "));
        m_output->setTextColor(QColor(Qt::darkBlue));
        print(s);
        m_output->setTextColor(QColor(Qt::black));
    }
}

void MainWindow::slotProcessFinished(int code,QProcess::ExitStatus status)
{
    if(status==QProcess::NormalExit)
    {
        print(tr("Proceso acabado correctamente. Valor de retorno: ")+QString::number(code));
    }
    else
    {
        print(tr("El proceso ha fallado inesperadamente. Salida: "+code));
    }
    finishProgressBar();
}

void MainWindow::slotProcessError(QProcess::ProcessError error)
{
    m_output->setTextColor(QColor(Qt::darkRed));
    switch(error)
    {
        case QProcess::FailedToStart:
            print(tr("Imposible lanzar el proceso."));
            break;
        case QProcess::WriteError:
            print(tr("Error de escritura en el proceso."));
            break;
        case QProcess::ReadError:
            print(tr("Error de lectura del proceso."));
            break;
        // No capturo crashed porque la pillo por finished
        case QProcess::Crashed:
        case QProcess::Timedout:
            break;
        case QProcess::UnknownError:
        default:
            print(tr("Error desconocido."));
            break;
    }
    finishProgressBar();
    m_output->setTextColor(QColor(Qt::black));
}

void MainWindow::slotCreateTerminal()
{
    QTermWidget* console = new QTermWidget(1,this);
    QFont font = QApplication::font();
    font.setFamily("DejaVu Sans Mono");
    font.setPointSize(12);
    
    console->setTerminalFont(font);
    console->setFocusPolicy(Qt::StrongFocus);
    
    //console->setColorScheme(COLOR_SCHEME_BLACK_ON_LIGHT_YELLOW);
    console->setScrollBarPosition(QTermWidget::ScrollBarRight);

    ++m_numberTerminal;

    connect(console,SIGNAL(finished()),this,SLOT(slotDeleteTerminal()));

    QString name=tr("Term ")+QString::number(m_numberTerminal);
    m_tabs->addTab(console,name);
}

void MainWindow::slotDeleteTerminal()
{
    QWidget *widget = qobject_cast<QWidget *>(sender());
    Q_ASSERT(widget);
    m_tabs->removeTab(m_tabs->indexOf(widget));
    delete widget;
}

void MainWindow::slotWebBarReturnPressed()
{
    QUrl url(m_webBar->text());
    if(url.isValid())
      slotLinkHandle(url);
}

int MainWindow::readEnvironmentValues()
{
    // The return value
    int ret=true;

    // Get all environment variables
    QStringList environmentlist=QProcess::systemEnvironment();
    // This is the list of the important variables
    QStringList variablelist=QString(ENVIRONMENT).split(",");

    // This is an auxiliar variable
    QStringList stringlist;

    foreach (QString str,variablelist)
    {
        // Look for the variable in the environment 
        stringlist=environmentlist.filter(str+"=");

        if(stringlist.isEmpty())
        {
            m_env[str]="";
            ret=false;
        }
        else
        {
            // Get the first element and get the value part
            m_env[str]=(stringlist.first().split("="))[1];
        }
    }

    return ret;
}

void MainWindow::print(QString s)
{
  if(!s.endsWith("\n"))
    s+="\n";
  if(m_logstream)
    *m_logstream<<CURRENT_TIME()<<": "<<s;
  if(m_output)
    m_output->insertPlainText(s);
}

void MainWindow::captureOutputForStatusBar(QString output)
{
  // Capturar para modificar status bar
  output=output.trimmed();

  QRegExp regexp(REGEXP_STRING);
  if(regexp.indexIn(output) != -1)
  {
    int pass=regexp.cap(1).toInt();
    output.replace(regexp,"");
    m_progressBar->setValue(pass);
    m_progressBar->setFormat("%p%"+output);
  }
}

void MainWindow::startProgressBar()
{
    QStatusBar* st=statusBar();
    st->clearMessage();
    st->addWidget(m_progressBar,100);
    m_progressBar->show();
    m_web->setEnabled(false);
}

void MainWindow::finishProgressBar()
{
    QStatusBar* st=statusBar();
    st->removeWidget(m_progressBar);
    st->showMessage(tr("Listo"));
    m_web->setEnabled(true);
}


void MainWindow::executeCommand(QString &string)
{
    QStringList list=string.split(" ",QString::SkipEmptyParts);
    QString program=list.takeFirst();
    m_process->setReadChannel(QProcess::StandardOutput);
    // Le ponemos el mismo entorno que tiene el browser ahora mismo
    m_process->setEnvironment(QProcess::systemEnvironment());
    m_process->start(program,list);
    m_output->insertPlainText(tr("Lanzando el comando: "));
    m_output->setTextColor(QColor(Qt::darkGreen));
    print(program+" "+list.join(" "));
    m_output->setTextColor(QColor(Qt::black));
    startProgressBar();
}
