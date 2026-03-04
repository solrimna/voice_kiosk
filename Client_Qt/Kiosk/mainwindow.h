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
#include <QMessageBox>              // 메시지 박스용
#include <QDebug>                   // 디버그 출력용

#include <speechapi_cxx.h>          // Azure Speech SDK 추가!
#include <QTimer>                   // 타이머용
#include <memory>                   // std::shared_ptr용

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
    void onStartRecord();                           // Azure 실시간으로 변경 예정!
    void onStopRecord();                            // Azure 중지로 변경 예정!

    // 파일 업로드용
    void onUploadFinished(QNetworkReply *reply);

    // Azure Speech 테스트 슬롯
    void onTestAzureSpeech();

    void onRecognizingText(const QString& text);    // 중간 결과 (실시간)
    void onRecognizedText(const QString& text);     // 최종 결과
    void onRecognitionError(const QString& error);  // 에러 처리

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

    // ─── Azure Speech 실시간 인식 관련 ──────────────────────────
    std::shared_ptr<Microsoft::CognitiveServices::Speech::SpeechRecognizer> azureRecognizer;
    bool isRecognizing;                             // 중복 실행 방지용 / 현재 인식 중인지
    QString recognizedText;                         // 누적된 최종 텍스트

    void sendTextToDjango(const QString& text);     // 최종 텍스트를 Django로 전송
};
#endif // MAINWINDOW_H
