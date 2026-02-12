#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMediaCaptureSession>     // 오디오 입력과 녹음기를 연결하기 위해 추가(중간 관리자, 카메라/화면 녹화도 가능)
#include <QAudioInput>              // 마이크 입력을 위한 추가
#include <QMediaRecorder>           // 실제 녹음 수행/파일로 저장 지원을 위해 추가
#include <QHttpMultiPart>           // 파일 전송을 위해 추가
#include <QMediaPlayer>             // 파일 재생을 위해 추가
#include <QAudioOutput>             // 파일 재생을 위해 추가

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartButtonClicked();
    void onNetworkReply(QNetworkReply *reply);

    // 녹음 관련 슬롯 추가!
    void onStartRecord();
    void onStopRecord();

    // 파일 업로드용
    void onUploadFinished(QNetworkReply *reply);

private:
    Ui::MainWindow *ui;
    QNetworkAccessManager *networkManager;
    QNetworkAccessManager *uploadManager;           // 업로드 전용

    // 오디오 녹음 관련 멤버 변수
    QMediaCaptureSession *captureSession;
    QAudioInput *audioInput;
    QMediaRecorder *mediaRecorder;

    QString lastRecordedFile;                       // 마지막 녹음 파일 경로 저장

    void uploadVoiceFile(const QString &filePath);  // 파일 업로드 함수

    // TTS 재생하기위해 추가
    QMediaPlayer *mediaPlayer;
    QAudioOutput *audioOutput;
};
#endif // MAINWINDOW_H
