#ifndef QGSRECT_H
#define QGSRECT_H
/*! \class QgsRect
 * \brief A rectangle specified with double values.
 *
 * QgsRect is used to store a rectangle when double values are required. 
 * Examples are storing a layer extent or the current view extent of a map
 */
class QgsRect{
 public:
    //! Constructor
    QgsRect(double xmin=0, double ymin=0, double xmax=0, double ymax=0);
    //! Destructor
    ~QgsRect();
    //! Set the minimum x value
    void setXmin(double x);
    //! Set the maximum x value
    void setXmax(double x);
    //! Set the maximum y value
    void setYmin(double y);
    //! Set the maximum y value
    void setYmax(double y);
    //! Get the x maximum value (right side of rectangle)
    double xMax() const;
    //! Get the x maximum value (right side of rectangle)
    double xMin() const;
    //! Get the x minimum value (left side of rectangle)
    double yMax() const;
    //! Get the y maximum value (top side of rectangle)
    double yMin() const;
    //! Normalize the rectangle so it has non-negative width/height
    void normalize();
    /*! Comparison operator
      @return True if rectangles are equal
    */
    bool operator==(const QgsRect &r1);
    /*! Assignment operator
     * @param r1 QgsRect to assign from
     */
    QgsRect & operator=(const QgsRect &r1);
 private:
    double xmax;
    double xmin;
    double ymax;
    double ymin;
};
#endif // QGSRECT_H
