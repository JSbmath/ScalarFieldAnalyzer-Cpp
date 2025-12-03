#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <set>
#include <tuple>
#include <stdexcept>
#include <algorithm>
#include <cmath>

using namespace std;


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // Start with the isovalue field disabled by default
    ui->isovalueLineEdit->setEnabled(false);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_browseButton_clicked()
{
    // This opens a standard file.
    QString filePath = QFileDialog::getOpenFileName(this, "Open Data File", "", "CSV Files (*.csv);;All Files (*)");

    if (!filePath.isEmpty()) {
        // Set the text of our line edit widget to the selected file path.
        ui->filePathLineEdit->setText(filePath);
    }
}


void MainWindow::on_isoRadioButton_toggled(bool checked)
{
    // Pass this value directly to the setEnabled
    ui->isovalueLineEdit->setEnabled(checked);
}


void MainWindow::on_processButton_clicked()
{
    // 1- Check if input is correct
    QString filePath = ui->filePathLineEdit->text();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please select a data file first.");
        return;
    }

    // Handle when there is an error
    GridData grid;
    try {
        // Convert the QString to string
        grid = loadScalarField(filePath.toStdString());
    } catch (const std::runtime_error& e) {
        QMessageBox::critical(this, "File Error", "Could not load or process the file.\nError: " + QString(e.what()));
        return;
    }

    // 2- Check the operation we are going to execute
    QString results; 

    if (ui->isoRadioButton->isChecked()) {
        QString isoText = ui->isovalueLineEdit->text();
        bool isNumeric;
        double isovalue = isoText.toDouble(&isNumeric);

        if (!isNumeric || isoText.isEmpty()) {
            QMessageBox::warning(this, "Input Error", "Please enter a valid numeric isovalue.");
            return;
        }

        results.append(QString("Iso-contour segments for isovalue %1:\n\n").arg(isovalue));
         // We will use the Marching Squares algorithm to get the isocontours.
        vector<LineSegment> segments = marchingSquares(grid, isovalue);
        for (const auto& seg : segments) {
            results.append(QString("Segment: (%1, %2) -> (%3, %4)\n")
                               .arg(seg.p1.x, 0, 'f', 6)
                               .arg(seg.p1.y, 0, 'f', 6)
                               .arg(seg.p2.x, 0, 'f', 6)
                               .arg(seg.p2.y, 0, 'f', 6));
        }

    } else {
        // We will use a neighborhood analysis to find the critical points.
        vector<CriticalPoint> points = findCriticalPoints(grid);

        if (ui->minimaRadioButton->isChecked()) {
            results.append("Local Minima Found:\n\n");
            for (const auto& p : points) {
                if (p.type == LOCAL_MINIMA) {
                    results.append(QString("Point: (%1, %2) | Value: %3\n")
                                       .arg(p.x, 0, 'f', 6)
                                       .arg(p.y, 0, 'f', 6)
                                       .arg(p.s_value, 0, 'f', 6));
                }
            }
        } else if (ui->maximaRadioButton->isChecked()) {
            results.append("Local Maxima Found:\n\n");
            for (const auto& p : points) {
                if (p.type == LOCAL_MAXIMA) {
                    results.append(QString("Point: (%1, %2) | Value: %3\n")
                                       .arg(p.x, 0, 'f', 6)
                                       .arg(p.y, 0, 'f', 6)
                                       .arg(p.s_value, 0, 'f', 6));
                }
            }
        } else if (ui->saddleRadioButton->isChecked()) {
            results.append("Saddle Points Found:\n\n");
            for (const auto& p : points) {
                if (p.type == SADDLE_POINT) {
                    results.append(QString("Point: (%1, %2) | Value: %3\n")
                                       .arg(p.x, 0, 'f', 6)
                                       .arg(p.y, 0, 'f', 6)
                                       .arg(p.s_value, 0, 'f', 6));
                }
            }
        } else {
            QMessageBox::warning(this, "Input Error", "Please select an operation to perform.");
            return;
        }
    }

    // 3. Display results
    ui->resultsTextEdit->setPlainText(results);
}



// Loads the 2D scalar field from a CSV file.
GridData MainWindow::loadScalarField(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        throw runtime_error("Could not open file " + filename);
    }

    vector<tuple<double, double, double>> points;
    set<double> unique_x, unique_y;

    string line;
    getline(file, line); // Skip the "x,y,S" header

    while (getline(file, line)) {
        stringstream ss(line);
        string x_str, y_str, s_str;

        getline(ss, x_str, ',');
        getline(ss, y_str, ',');
        getline(ss, s_str, ',');

        if (!x_str.empty() && !y_str.empty() && !s_str.empty()) {
            double x = stod(x_str);
            double y = stod(y_str);
            double s = stod(s_str);
            points.emplace_back(x, y, s);
            unique_x.insert(x);
            unique_y.insert(y);
        }
    }

    GridData grid;
    grid.width = unique_x.size();
    grid.height = unique_y.size();
    grid.x_coords.assign(unique_x.begin(), unique_x.end());
    grid.y_coords.assign(unique_y.begin(), unique_y.end());

    grid.s_values.resize(grid.height, vector<double>(grid.width));

    for (const auto& p : points) {
        double x = get<0>(p);
        double y = get<1>(p);
        double s = get<2>(p);

        auto it_x = lower_bound(grid.x_coords.begin(), grid.x_coords.end(), x);
        auto it_y = lower_bound(grid.y_coords.begin(), grid.y_coords.end(), y);

        if (it_x != grid.x_coords.end() && it_y != grid.y_coords.end()) {
            size_t ix = distance(grid.x_coords.begin(), it_x);
            size_t iy = distance(grid.y_coords.begin(), it_y);
            grid.s_values[iy][ix] = s;
        }
    }

    return grid;
}

// Calculates the position of a point on an edge using linear interpolation.
Point MainWindow::linearInterpolate(const Point& p1, const Point& p2, double val1, double val2, double isovalue) {
    if (abs(val1 - val2) < 1e-9) {
        return p1;
    }
    double t = (isovalue - val1) / (val2 - val1);
    return {p1.x + t * (p2.x - p1.x), p1.y + t * (p2.y - p1.y)};
}

// Implements the Marching Squares algorithm to find the contours.
vector<LineSegment> MainWindow::marchingSquares(const GridData& grid, double isovalue) {
    vector<LineSegment> segments;

    for (int j = 0; j < grid.height - 1; ++j) {
        for (int i = 0; i < grid.width - 1; ++i) {
            // Points and values of the current cell
            //  p4---p3
            //  |     |
            //  p1---p2
            Point p1 = {grid.x_coords[i], grid.y_coords[j]};
            Point p2 = {grid.x_coords[i+1], grid.y_coords[j]};
            Point p3 = {grid.x_coords[i+1], grid.y_coords[j+1]};
            Point p4 = {grid.x_coords[i], grid.y_coords[j+1]};

            double v1 = grid.s_values[j][i];
            double v2 = grid.s_values[j][i+1];
            double v3 = grid.s_values[j+1][i+1];
            double v4 = grid.s_values[j+1][i];

            // Determine the cell's index (which vertices are "inside")
            int square_index = 0;
            if (v1 > isovalue) square_index |= 1; // Bit 0
            if (v2 > isovalue) square_index |= 2; // Bit 1
            if (v3 > isovalue) square_index |= 4; // Bit 2
            if (v4 > isovalue) square_index |= 8; // Bit 3

            /*
 * Superior Edge
 * p4 ------ b ------ p3
 * |                  |
 * |                  |
 * Left Edge (a)      (c) Right Edge
 * |                  |
 * |                  |
 * p1 ------ d ------ p2
 * Lower edge
 */

            // Interpolate to find the points on the edges
            Point a = linearInterpolate(p1, p4, v1, v4, isovalue); // Left edge
            Point b = linearInterpolate(p4, p3, v4, v3, isovalue); // Top edge
            Point c = linearInterpolate(p2, p3, v2, v3, isovalue); // Right edge
            Point d = linearInterpolate(p1, p2, v1, v2, isovalue); // Bottom edge

            // Create segments based on the index
            switch (square_index) {
            case 1: case 14: segments.push_back({a, d}); break;
            case 2: case 13: segments.push_back({c, d}); break;
            case 3: case 12: segments.push_back({a, c}); break;
            case 4: case 11: segments.push_back({b, c}); break;
            case 5:          segments.push_back({a, d}); segments.push_back({b, c}); break; // Ambiguous case
            case 6: case 9:  segments.push_back({b, d}); break;
            case 7: case 8:  segments.push_back({a, b}); break;
            case 10:         segments.push_back({a, b}); segments.push_back({c, d}); break; // Ambiguous case
                // Cases 0 and 15 do not generate lines
            }
        }
    }
    return segments;
}

// Analyzes the grid to find local minima, local maxima, and saddle points.
vector<CriticalPoint> MainWindow::findCriticalPoints(const GridData& grid) {
    vector<CriticalPoint> points;

    // We only iterate over the interior points, as the border points do not have 8 neighbors.
    for (int j = 1; j < grid.height - 1; ++j) {
        for (int i = 1; i < grid.width - 1; ++i) {
            double center_val = grid.s_values[j][i];

            // Collect the 8 neighbors
            double neighbors[8] = {
                grid.s_values[j-1][i-1], grid.s_values[j-1][i], grid.s_values[j-1][i+1],
                grid.s_values[j][i-1],                         grid.s_values[j][i+1],
                grid.s_values[j+1][i-1], grid.s_values[j+1][i], grid.s_values[j+1][i+1]
            };

            // Check if it is a local maximum (strictly greater than all neighbors)
            bool is_max = true;
            for(double n : neighbors) {
                if (center_val <= n) {
                    is_max = false;
                    break;
                }
            }
            if (is_max) {
                points.push_back({grid.x_coords[i], grid.y_coords[j], center_val, LOCAL_MAXIMA});
                continue; // We have classified it, so we move to the next point
            }

            // Check if it is a local minimum 
            bool is_min = true;
            for(double n : neighbors) {
                if (center_val >= n) {
                    is_min = false;
                    break;
                }
            }
            if (is_min) {
                points.push_back({grid.x_coords[i], grid.y_coords[j], center_val, LOCAL_MINIMA});
                continue;
            }

            // Check if it is a saddle point.
            // A robust definition on a grid is to count the sign changes
            // when walking around the point. A saddle point has 4 or more changes.
            double ordered_neighbors[8] = {
                grid.s_values[j-1][i],   // North
                grid.s_values[j-1][i+1], // Northeast
                grid.s_values[j][i+1],   // East
                grid.s_values[j+1][i+1], // Southeast
                grid.s_values[j+1][i],   // South
                grid.s_values[j+1][i-1], // Southwest
                grid.s_values[j][i-1],   // West
                grid.s_values[j-1][i-1]  // Northwest
            };

            int sign_changes = 0;
            // We compare each neighbor with the central point to see if it's greater (1) or smaller (-1)
            int first_sign = (ordered_neighbors[0] > center_val) ? 1 : -1;
            int last_sign = first_sign;

            for (int k = 1; k < 8; ++k) {
                int current_sign = (ordered_neighbors[k] > center_val) ? 1 : -1;
                if (current_sign != last_sign) {
                    sign_changes++;
                }
                last_sign = current_sign;
            }
            // We compare the last with the first to close the loop
            if (last_sign != first_sign) {
                sign_changes++;
            }

            if (sign_changes >= 4) {
                points.push_back({grid.x_coords[i], grid.y_coords[j], center_val, SADDLE_POINT});
            }
        }
    }
    return points;
}