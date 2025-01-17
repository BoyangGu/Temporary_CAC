/***************************************************************************
                                 ucl_d_mat.h
                             -------------------
                               W. Michael Brown

  Matrix Container on Device

 __________________________________________________________________________
    This file is part of the Geryon Unified Coprocessor Library (UCL)
 __________________________________________________________________________

    begin                : Thu Jun 25 2009
    copyright            : (C) 2009 by W. Michael Brown
    email                : brownw@ornl.gov
 ***************************************************************************/

/* -----------------------------------------------------------------------
   Copyright (2009) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the Simplified BSD License.
   ----------------------------------------------------------------------- */

// Only allow this file to be included by CUDA and OpenCL specific headers
#ifdef _UCL_MAT_ALLOW

/// 2D Matrix on device (can have extra column storage to get correct alignment)
template <class numtyp>
class UCL_D_Mat : public UCL_BaseMat {
 public:
  // Traits for copying data
  // MEM_TYPE is 0 for device, 1 for host, and 2 for image
  enum traits {
    DATA_TYPE = _UCL_DATA_ID<numtyp>::id,
    MEM_TYPE = 0,
    PADDED = 1,
    ROW_MAJOR = 1,
    VECTOR = 0
  };
  typedef numtyp data_type;

  UCL_D_Mat() : _cols(0) {}
  ~UCL_D_Mat() { _device_free(*this); }

  /// Construct with specified rows and cols
  /** \sa alloc() **/
  UCL_D_Mat(const size_t rows, const size_t cols, UCL_Device &device,
            const enum UCL_MEMOPT kind=UCL_READ_WRITE) :
    _cols(0) { alloc(rows,cols,device,kind); }

  /// Row major matrix on device
  /** The kind parameter controls memory optimizations as follows:
    * - UCL_READ_WRITE - Specify that you will read and write in kernels
    * - UCL_WRITE_ONLY - Specify that you will only write in kernels
    * - UCL_READ_ONLY  - Specify that you will only read in kernels
    * \param cq Default command queue for operations copied from another mat
    * \note - Coalesced access using adjacent cols on same row
    *         UCL_D_Mat(row,col) given by array[row*row_size()+col]
    * \return UCL_SUCCESS if the memory allocation is successful **/
  template <class mat_type>
  inline int alloc(const size_t rows, const size_t cols, mat_type &cq,
                   const enum UCL_MEMOPT kind=UCL_READ_WRITE) {
    clear();

    int err=_device_alloc(*this,cq,rows,cols,_pitch,kind);
    if (err!=UCL_SUCCESS) {
      #ifndef UCL_NO_EXIT
      std::cerr << "UCL Error: Could not allocate "
                << rows*cols*sizeof(numtyp) << " bytes on device.\n";
      UCL_GERYON_EXIT;
      #endif
      return err;
    }

    _kind=kind;
    _rows=rows;
    _cols=cols;
    _row_size=_pitch/sizeof(numtyp);
    #ifndef _UCL_DEVICE_PTR_MAT
    _end=_array+_row_size*cols;
    #endif
    #ifdef _OCL_MAT
    _offset=0;
    #endif
    return err;
  }

  /// Row major matrix on device
  /** The kind parameter controls memory optimizations as follows:
    * - UCL_READ_WRITE - Specify that you will read and write in kernels
    * - UCL_WRITE_ONLY - Specify that you will only write in kernels
    * - UCL_READ_ONLY  - Specify that you will only read in kernels
    * \param device Used to get the default command queue for operations
    * \note - Coalesced access using adjacent cols on same row
    *         UCL_D_Mat(row,col) given by array[row*row_size()+col]
    * \return UCL_SUCCESS if the memory allocation is successful **/
  inline int alloc(const size_t rows, const size_t cols, UCL_Device &device,
                   const enum UCL_MEMOPT kind=UCL_READ_WRITE) {
    clear();

    int err=_device_alloc(*this,device,rows,cols,_pitch,kind);
    if (err!=UCL_SUCCESS) {
      #ifndef UCL_NO_EXIT
      std::cerr << "UCL Error: Could not allocate "
                << rows*cols*sizeof(numtyp) << " bytes on device.\n";
      UCL_GERYON_EXIT;
      #endif
      return err;
    }

    _kind=kind;
    _rows=rows;
    _cols=cols;
    _row_size=_pitch/sizeof(numtyp);
    #ifndef _UCL_DEVICE_PTR_MAT
    _end=_array+_row_size*cols;
    #endif
    #ifdef _OCL_MAT
    _offset=0;
    #endif
    return err;
  }

  /// Do not allocate memory, instead use an existing allocation from Geryon
  /** This function must be passed a Geryon vector or matrix container.
    * No memory is freed when the object is destructed.
    * - The view does not prevent the memory from being freed by the
    *   allocating container when using CUDA APIs
    * \param stride Number of _elements_ between the start of each row **/
  template <class ucl_type>
  inline void view(ucl_type &input, const size_t rows, const size_t cols,
                   const size_t stride) {
    clear();
    _kind=UCL_VIEW;
    _rows=rows;
    _cols=cols;
    _pitch=stride*sizeof(numtyp);
    _row_size=stride;
    this->_cq=input.cq();
    #ifdef _OCL_MAT
    _offset=input.offset();
    _array=input.cbegin();
    CL_SAFE_CALL(clRetainMemObject(input.cbegin()));
    CL_SAFE_CALL(clRetainCommandQueue(input.cq()));
    #else
    _device_view(&_array,input.begin());
    #endif

    #ifndef _UCL_DEVICE_PTR_MAT
    _end=_array+_cols;
    #endif
  }

  /// Do not allocate memory, instead use an existing allocation from Geryon
  /** This function must be passed a Geryon vector or matrix container.
    * No memory is freed when the object is destructed.
    * - The view does not prevent the memory from being freed by the
    *   allocating container when using CUDA APIs **/
  template <class ucl_type>
  inline void view(ucl_type &input, const size_t rows, const size_t cols)
    { view(input,rows,cols,input.row_size()); }

  /// Do not allocate memory, instead use an existing allocation from Geryon
  /** This function must be passed a Geryon vector or matrix container.
    * No memory is freed when the object is destructed.
    * - The view does not prevent the memory from being freed by the
    *   allocating container when using CUDA APIs
    * - If a matrix is used a input, all elements (including padding)
    *   will be used for view **/
  template <class ucl_type>
  inline void view(ucl_type &input, const size_t cols)
    { view(input,1,cols); }

  /// Do not allocate memory, instead use an existing allocation from Geryon
  /** This function must be passed a Geryon vector or matrix container.
    * No memory is freed when the object is destructed.
    * - The view does not prevent the memory from being freed by the
    *   allocating container when using CUDA APIs
    * - If a matrix is used a input, all elements (including padding)
    *   will be used for view **/
  template <class ucl_type>
  inline void view(ucl_type &input)
    { view(input,input.rows(),input.cols()); }

  /// Do not allocate memory, instead use an existing allocation
  /** - No memory is freed when the object is destructed.
    * - The view does not prevent the memory from being freed by the
    *   allocating container when using CUDA APIs
    * \param stride Number of _elements_ between the start of each row **/
  template <class ptr_type>
  inline void view(ptr_type input, const size_t rows, const size_t cols,
                   const size_t stride, UCL_Device &dev) {
    clear();
    _kind=UCL_VIEW;
    _cols=cols;
    _rows=rows;
    _pitch=stride*sizeof(numtyp);
    _row_size=stride;
    this->_cq=dev.cq();
    _array=input;
    #ifndef _UCL_DEVICE_PTR_MAT
    _end=_array+_cols;
    #endif
    #ifdef _OCL_MAT
    _offset=0;
    CL_SAFE_CALL(clRetainMemObject(input));
    CL_SAFE_CALL(clRetainCommandQueue(dev.cq()));
    #endif
  }

  /// Do not allocate memory, instead use an existing allocation
  /** - No memory is freed when the object is destructed.
    * - The view does not prevent the memory from being freed by the
    *   allocating container when using CUDA APIs **/
  template <class ptr_type>
  inline void view(ptr_type input, const size_t rows, const size_t cols,
                   UCL_Device &dev) { view(input,rows,cols,cols,dev); }

  /// Do not allocate memory, instead use an existing allocation
  /** - No memory is freed when the object is destructed.
    * - The view does not prevent the memory from being freed by the
    *   allocating container when using CUDA APIs **/
  template <class ptr_type>
  inline void view(ptr_type input, const size_t cols, UCL_Device &dev)
    { view(input,1,cols,dev); }

  /// Do not allocate memory, instead use an existing allocation from Geryon
  /** This function must be passed a Geryon vector or matrix container.
    * No memory is freed when the object is destructed.
    * - The view does not prevent the memory from being freed by the
    *   allocating container when using CUDA APIs
    * \param stride Number of _elements_ between the start of each row **/
  template <class ucl_type>
  inline void view_offset(const size_t offset,ucl_type &input,const size_t rows,
                          const size_t cols, const size_t stride) {
    clear();
    _kind=UCL_VIEW;
    _cols=cols;
    _rows=rows;
    _pitch=stride*sizeof(numtyp);
    _row_size=stride;
    this->_cq=input.cq();
    #ifdef _OCL_MAT
    _array=input.begin();
    _offset=offset+input.offset();
    CL_SAFE_CALL(clRetainMemObject(input.cbegin()));
    CL_SAFE_CALL(clRetainCommandQueue(input.cq()));
    #else
    _device_view(&_array,input.begin(),offset,sizeof(numtyp));
    #endif

    #ifndef _UCL_DEVICE_PTR_MAT
    _end=_array+_cols;
    #endif
  }

  /// Do not allocate memory, instead use an existing allocation from Geryon
  /** This function must be passed a Geryon vector or matrix container.
    * No memory is freed when the object is destructed.
    * - The view does not prevent the memory from being freed by the
    *   allocating container when using CUDA APIs **/
  template <class ucl_type>
  inline void view_offset(const size_t offset,ucl_type &input,const size_t rows,
                          const size_t cols)
    { view_offset(offset,input,rows,cols,input.row_size()); }

  /// Do not allocate memory, instead use an existing allocation from Geryon
  /** This function must be passed a Geryon vector or matrix container.
    * No memory is freed when the object is destructed.
    * - The view does not prevent the memory from being freed by the
    *   allocating container when using CUDA APIs
    * - If a matrix is used a input, all elements (including padding)
    *   will be used for view **/
  template <class ucl_type>
  inline void view_offset(const size_t offset,ucl_type &input,const size_t cols)
    { view_offset(offset,input,1,cols); }

  /// Do not allocate memory, instead use an existing allocation from Geryon
  /** This function must be passed a Geryon vector or matrix container.
    * No memory is freed when the object is destructed.
    * - The view does not prevent the memory from being freed by the
    *   allocating container when using CUDA APIs
    * - If a matrix is used a input, all elements (including padding)
    *   will be used for view **/
  template <class ucl_type>
  inline void view_offset(const size_t offset, ucl_type &input) {
    if (input.rows()==1)
      view_offset(offset,input,1,input.cols()-offset);
    else
      view_offset(offset,input,input.rows()-offset/input.row_size(),
                  input.cols());
  }

  /// Do not allocate memory, instead use an existing allocation
  /** - No memory is freed when the object is destructed.
    * - The view does not prevent the memory from being freed by the
    *   allocating container when using CUDA APIs
    * \param stride Number of _elements_ between the start of each row **/
  template <class ptr_type>
  inline void view_offset(const size_t offset,ptr_type input,const size_t rows,
                          const size_t cols,const size_t stride,
                          UCL_Device &dev) {
    clear();
    _kind=UCL_VIEW;
    _cols=cols;
    _rows=rows;
    _pitch=stride*sizeof(numtyp);
    _row_size=stride;
    this->_cq=dev.cq();

    #ifdef _OCL_MAT
    _array=input;
    _offset=offset;
    CL_SAFE_CALL(clRetainMemObject(input));
    CL_SAFE_CALL(clRetainCommandQueue(dev.cq()));
    #else
    #ifdef _UCL_DEVICE_PTR_MAT
    _array=input+offset*sizeof(numtyp);
    #else
    _array=input+offset;
    #endif
    #endif

    #ifndef _UCL_DEVICE_PTR_MAT
    _end=_array+_cols;
    #endif
  }

  /// Do not allocate memory, instead use an existing allocation
  /** - No memory is freed when the object is destructed.
    * - The view does not prevent the memory from being freed by the
    *   allocating container when using CUDA APIs **/
  template <class ptr_type>
  inline void view_offset(const size_t offset,ptr_type input,const size_t rows,
                          const size_t cols, UCL_Device &dev)
    { view_offset(offset,input,rows,cols,cols,dev); }

  /// Do not allocate memory, instead use an existing allocation
  /** - No memory is freed when the object is destructed.
    * - The view does not prevent the memory from being freed by the
    *   allocating container when using CUDA APIs **/
  template <class ptr_type>
  inline void view_offset(const size_t offset, ptr_type input,
                          const size_t cols, UCL_Device &dev)
    { view_offset(offset,input,1,cols,dev); }

  /// Free memory and set size to 0
  inline void clear()
    { _device_free(*this); _cols=0; _kind=UCL_VIEW; }

  /// Resize the allocation to contain cols elements
  /** \note Cannot be used on views **/
  inline int resize(const int rows, const int cols) {
    assert(_kind!=UCL_VIEW);

    int err=_device_resize(*this,rows,cols,_pitch);
    if (err!=UCL_SUCCESS) {
      #ifndef UCL_NO_EXIT
      std::cerr << "UCL Error: Could not allocate "
                << rows*cols*sizeof(numtyp) << " bytes on device.\n";
      UCL_GERYON_EXIT;
      #endif
      return err;
    }

    _rows=rows;
    _cols=cols;
    _row_size=_pitch/sizeof(numtyp);
    #ifndef _UCL_DEVICE_PTR_MAT
    _end=_array+_row_size*cols;
    #endif
    #ifdef _OCL_MAT
    _offset=0;
    #endif
    return err;
  }

  /// Resize (only if bigger) the allocation to contain rows x cols elements
  /** \note Cannot be used on views **/
  inline int resize_ib(const int rows, const int cols)
    { if (cols>_cols || rows>_rows) return resize(rows,cols);
      else return UCL_SUCCESS; }

  /// Set each element to zero asynchronously in the default command_queue
  inline void zero() { zero(_cq); }
  /// Set first n elements to zero asynchronously in the default command_queue
  inline void zero(const int n) { zero(n,_cq); }
  /// Set each element to zero asynchronously
  inline void zero(command_queue &cq)
    { _device_zero(*this,row_bytes()*_rows,cq); }
  /// Set first n elements to zero asynchronously
  inline void zero(const int n, command_queue &cq)
    { _device_zero(*this,n*sizeof(numtyp),cq); }


  #ifdef _UCL_DEVICE_PTR_MAT
  /// For OpenCL, returns a (void *) device pointer to memory allocation
  inline device_ptr & begin() { return _array; }
  /// For OpenCL, returns a (void *) device pointer to memory allocation
  inline const device_ptr & begin() const { return _array; }
  #else
  /// For CUDA-RT, get device pointer to first element
  inline numtyp * & begin() { return _array; }
  /// For CUDA-RT, get device pointer to first element
  inline numtyp * const & begin() const { return _array; }
  /// For CUDA-RT, get device pointer to one past last element
  inline numtyp * end() { return _end; }
  /// For CUDA-RT, get device pointer to one past last element
  inline const numtyp * end() const { return _end; }
  #endif

  #ifdef _UCL_DEVICE_PTR_MAT
  /// Returns an API specific device pointer
  /** - For OpenCL, returns a &cl_mem object
    * - For CUDA Driver, returns a &CUdeviceptr
    * - For CUDA-RT, returns void** **/
  inline device_ptr & cbegin() { return _array; }
  /// Returns an API specific device pointer
  /** - For OpenCL, returns a &cl_mem object
    * - For CUDA Driver, returns a &CUdeviceptr
    * - For CUDA-RT, returns void** **/
  inline const device_ptr & cbegin() const { return _array; }
  #else
  /// Returns an API specific device pointer
  /** - For OpenCL, returns a &cl_mem object
    * - For CUDA Driver, returns a &CUdeviceptr
    * - For CUDA-RT, returns numtyp** **/
  inline numtyp ** cbegin() { return &_array; }
  /// Returns an API specific device pointer
  /** - For OpenCL, returns a &cl_mem object
    * - For CUDA Driver, returns a &CUdeviceptr
    * - For CUDA-RT, returns numtyp** **/
  inline const numtyp ** cbegin() const { return &_array; }
  #endif

  /// Get the number of elements
  inline size_t numel() const { return _cols*_rows; }
  /// Get the number of rows
  inline size_t rows() const { return _rows; }
  /// Get the number of columns
  inline size_t cols() const { return _cols; }
  ///Get the size of a row (including any padding) in elements
  inline size_t row_size() const { return _row_size; }
  /// Get the size of a row (including any padding) in bytes
  inline size_t row_bytes() const { return _pitch; }
  /// Get the size in bytes of 1 element
  inline int element_size() const { return sizeof(numtyp); }

  #ifdef _OCL_MAT
  /// Return the offset (in elements) from begin() pointer where data starts
  /** \note Always 0 for host matrices and CUDA APIs **/
  inline size_t offset() const { return _offset; }
  #else
  /// Return the offset (in elements) from begin() pointer where data starts
  /** \note Always 0 for host matrices and CUDA APIs **/
  inline size_t offset() const { return 0; }
  #endif

  /// Return the offset (in bytes) from begin() pointer where data starts
  /** \note Always 0 for host matrices and CUDA APIs **/
  inline size_t byteoff() const { return offset()*sizeof(numtyp); }

 private:
  size_t _pitch, _row_size, _rows, _cols;

  #ifdef _UCL_DEVICE_PTR_MAT
  device_ptr _array;
  #else
  numtyp *_array,*_end;
  #endif

  #ifdef _OCL_MAT
  size_t _offset;
  #endif
};

#endif

