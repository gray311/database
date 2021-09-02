//===----------------------------------------------------------------------===//
//
//
//
// p0_starter.h
//
// Identification: src/include/primer/p0_starter.h
//
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include <memory>
#include <stdexcept>
#include <vector>



namespace scudb {

/**
 * The Matrix type defines a common
 * interface for matrix operations.
 */
template <typename T>
class Matrix {
    protected:
        /**
        * TODO(P0): Add implementation
        * 
        * Construct a new Matrix instance.
        * @param rows The number of rows
        * @param cols The number of columns
        *
        */

        /** The number of rows in the matrix */
        int rows_;
        /** The number of columns in the matrix */
        int cols_;
        T *linear_;

        Matrix(int rows, int cols){
            rows_ = rows;
            cols_ = cols;
            linear_ = (T *)malloc(sizeof(T)*(rows_*cols_));
            return ;
        }
        /**
        * TODO(P0): Allocate the array in the constructor.
        * TODO(P0): Deallocate the array in the destructor.
        * A flattened array containing the elements of the matrix.
        */

    public:
        /** @return The number of rows in the matrix */
        virtual int GetRowCount() const{
            return rows_;
        }

        /** @return The number of columns in the matrix */
        virtual int GetColumnCount() const{
            return cols_;
        }

        /**
        * Get the (i,j)th matrix element.
        *
        * Throw OUT_OF_RANGE if either index is out of range.
        *
        * @param i The row index
        * @param j The column index
        * @return The (i,j)th matrix element
        * @throws OUT_OF_RANGE if either index is out of range
        */
        virtual T GetElement(int i, int j) const{
            if(i<0 || i>=rows_ || j<0 || j>=cols_) {
                printf("cross the index!");
            }
            else return linear_[i * cols_ + j];
        }

        /**
        * Set the (i,j)th matrix element.
        *
        * Throw OUT_OF_RANGE if either index is out of range.
        *
        * @param i The row index
        * @param j The column index
        * @param val The value to insert
        * @throws OUT_OF_RANGE if either index is out of range
        */
        virtual void SetElement(int i, int j, T val){
            if(i<0 || i>=rows_ || j<0 || j>=cols_) {
                printf("cross the index!");
                return ;
            }
            linear_[i * cols_ + j] = val;
            return ;
        }

        /**
        * Fill the elements of the matrix from `source`.
        *
        * Throw OUT_OF_RANGE in the event that `source`
        * does not contain the required number of elements.
        *
        * @param source The source container
        * @throws OUT_OF_RANGE if `source` is incorrect size
        */
        virtual void FillFrom(const std::vector<T> &source){
            for(int i = 0; i < source.size(); ++i)
            {
                linear_[i] = source[i];
            }
            return;
        }

        /**
        * Destroy a matrix instance.
        * TODO(P0): Add implementation
        */
        virtual ~Matrix(){
            delete linear_;
        }
};




/**
 * The RowMatrix type is a concrete matrix implementation.
 * It implements the interface defined by the Matrix type.
 */
template <typename T>
class RowMatrix : public Matrix<T> {
    private:
        /**
        * A 2D array containing the elements of the matrix in row-major format.
        *
        * TODO(P0):
        * - Allocate the array of row pointers in the constructor.
        * - Use these pointers to point to corresponding elements of the `linear` array.
        * - Don't forget to deallocate the array in the destructor.
        */
        T **data;
    public:
        /**
        * TODO(P0): Add implementation
        *
        * Construct a new RowMatrix instance.
        * @param rows The number of rows
        * @param cols The number of columns
        */
        RowMatrix(int rows, int cols) : Matrix<T>(rows, cols) {
            data = (T **)malloc(sizeof(T*)*Matrix<T>::rows_);
            for(int i=0;i<Matrix<T>::rows_;++i)
            {
                data[i]=(T *)malloc(sizeof(T)*Matrix<T>::cols_);
            }
        }

        /**
        * TODO(P0): Add implementation
        * @return The number of rows in the matrix
        */
        int GetRowCount() const override { return Matrix<T>::rows_; }

        /**
        * TODO(P0): Add implementation
        * @return The number of columns in the matrix
        */
        int GetColumnCount() const override { return Matrix<T>::cols_; }

        /**
        * TODO(P0): Add implementation
        *
        * Get the (i,j)th matrix element.
        *
        * Throw OUT_OF_RANGE if either index is out of range.
        *
        * @param i The row index
        * @param j The column index
        * @return The (i,j)th matrix element
        * @throws OUT_OF_RANGE if either index is out of range
        */
        T GetElement(int i, int j) const override {
        //throw NotImplementedException{"RowMatrix::GetElement() not implemented."};
            if(i<0 || i>=Matrix<T>::rows_ || j<0 || j>=Matrix<T>::cols_) {
                printf("cross the index!");
            }
            else return data[i][j];
        }

        /**
        * Set the (i,j)th matrix element.
        *
        * Throw OUT_OF_RANGE if either index is out of range.
        *
        * @param i The row index
        * @param j The column index
        * @param val The value to insert
        * @throws OUT_OF_RANGE if either index is out of range
        */
        void SetElement(int i, int j, T val) override {
            if(i<0 || i>=Matrix<T>::rows_ || j<0 || j>=Matrix<T>::cols_) {
                printf("cross the index!");
                return ;
            }
            data[i][j] = val;
            return ;
        }

        /**
        * TODO(P0): Add implementation
        *
        * Fill the elements of the matrix from `source`.
        *
        * Throw OUT_OF_RANGE in the event that `source`
        * does not contain the required number of elements.
        *
        * @param source The source container
        * @throws OUT_OF_RANGE if `source` is incorrect size
        */
        void FillFrom(const std::vector<T> &source) override {
            //throw NotImplementedException{"RowMatrix::FillFrom() not implemented."};
            int row=0,col=0;
            for(int i = 0; i < source.size(); ++i)
            {
                if(row>=Matrix<T>::rows_||col>=Matrix<T>::cols_) break;
                data[row][col] = source[i];
                if(col<Matrix<T>::cols_-1) col++;
                else if(row<Matrix<T>::rows_-1) col=0,row++;
            }
            return;
        }
        void Print() {
            for(int i=0;i<Matrix<T>::rows_;++i) {
                for(int j=0;j<Matrix<T>::cols_;++j) {
                    printf("%d ",data[i][j]);
                }
                printf("\n"); 
            }
        }

        /**
        * TODO(P0): Add implementation
        *
        * Destroy a RowMatrix instance.
        */
        ~RowMatrix() override{
            for(int i=0;i<Matrix<T>::rows_;++i) free(data[i]);
            free(data);
        }

    
};

/**
 * The RowMatrixOperations class defines operations
 * that may be performed on instances of `RowMatrix`.
 */
template <typename T>
class RowMatrixOperations {
    public:
        /**
        * Compute (`matrixA` + `matrixB`) and return the result.
        * Return `nullptr` if dimensions mismatch for input matrices.
        * @param matrixA Input matrix
        * @param matrixB Input matrix
        * @return The result of matrix addition
        */
        static std::unique_ptr<RowMatrix<T>> Add(const RowMatrix<T> *matrixA, const RowMatrix<T> *matrixB) {
            // TODO(P0): Add implementation
            if(matrixA->GetRowCount() != matrixB->GetRowCount() || matrixA->GetColumnCount() != matrixB->GetColumnCount())
                return std::unique_ptr<RowMatrix<T>>(nullptr);
            std::unique_ptr<RowMatrix<T> > matrixC(new RowMatrix<T>(matrixA->GetRowCount(),matrixA->GetColumnCount()));
            for(int i=0;i<matrixA->GetRowCount();++i)
                for(int j=0;j<matrixA->GetColumnCount();++j)
                    matrixC->SetElement(i,j, matrixA->GetElement(i,j) + matrixB->GetElement(i,j));
            return matrixC;
        }

        /**
        * Compute the matrix multiplication (`matrixA` * `matrixB` and return the result.
        * Return `nullptr` if dimensions mismatch for input matrices.
        * @param matrixA Input matrix
        * @param matrixB Input matrix
        * @return The result of matrix multiplication
        */
        static std::unique_ptr<RowMatrix<T>> Multiply(const RowMatrix<T> *matrixA, const RowMatrix<T> *matrixB) {
            // TODO(P0): Add implementation
            if(matrixA->GetColumnCount() != matrixB->GetRowCount())
                return std::unique_ptr<RowMatrix<T>>(nullptr);
            int n,m,q;
            std::unique_ptr<RowMatrix<T> > matrixC(new RowMatrix<T>(matrixA->GetRowCount(),matrixB->GetColumnCount()));
            n=matrixA->GetRowCount();
            m=matrixA->GetColumnCount();
            q=matrixB->GetColumnCount();
            for(int i=0;i<n;++i)
                for(int j=0;j<q;++j){
                    T temp;
                    for(int k=0;k<m;++k) temp += (matrixA->GetElement(i,k) * matrixB->GetElement(k,j));
                    matrixC->SetElement(i,j,temp);
                }
            return matrixC;
        }

        /**
        * Simplified General Matrix Multiply operation. Compute (`matrixA` * `matrixB` + `matrixC`).
        * Return `nullptr` if dimensions mismatch for input matrices.
        * @param matrixA Input matrix
        * @param matrixB Input matrix
        * @param matrixC Input matrix
        * @return The result of general matrix multiply
        */
        static std::unique_ptr<RowMatrix<T>> GEMM(const RowMatrix<T> *matrixA, const RowMatrix<T> *matrixB,
                                                const RowMatrix<T> *matrixC) {
            // TODO(P0): Add implementation
            if(matrixA->GetColumnCount() != matrixB->GetRowCount())
                return std::unique_ptr<RowMatrix<T>>(nullptr);
            std::unique_ptr<RowMatrix<T> > matrixD(new RowMatrix<T>(matrixA->GetRowCount(),matrixB->GetColumnCount()));
            matrixD = Multiply(matrixA,matrixB);
            const RowMatrix<T> *temp = matrixD.get();
            matrixD = Add(temp,matrixC);
            return matrixD;

        }
};
}  // namespace scudb
