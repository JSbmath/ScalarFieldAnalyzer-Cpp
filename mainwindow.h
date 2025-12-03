#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <vector>
#include <string>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// --- Estructuras de datos ---
struct Point { double x, y; };
struct LineSegment { Point p1, p2; };
enum PointType { LOCAL_MINIMA, LOCAL_MAXIMA, SADDLE_POINT };
struct CriticalPoint { double x, y; double s_value; PointType type; };
struct GridData {
    std::vector<std::vector<double>> s_values;
    std::vector<double> x_coords;
    std::vector<double> y_coords;
    int width;
    int height;
};

// --- Definición de la clase ---
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_browseButton_clicked();
    void on_isoRadioButton_toggled(bool checked);
    void on_processButton_clicked();

private:
    Ui::MainWindow *ui;

    // --- Declaraciones de las funciones de análisis ---
    GridData loadScalarField(const std::string& filename);
    std::vector<LineSegment> marchingSquares(const GridData& grid, double isovalue);
    Point linearInterpolate(const Point& p1, const Point& p2, double val1, double val2, double isovalue);
    std::vector<CriticalPoint> findCriticalPoints(const GridData& grid);
};

#endif // MAINWINDOW_H
