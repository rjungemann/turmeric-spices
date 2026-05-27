# tur-linalg Guide

Dense float linear algebra for Turmeric. This guide walks through the key
features of the `tur-linalg` spice: creating matrices and vectors, solving
linear systems, computing decompositions, and using the graphics helpers.

## Table of Contents

1. [Creating and manipulating matrices](#1-creating-and-manipulating-matrices)
2. [Solving linear systems](#2-solving-linear-systems)
3. [Least-squares fitting with QR](#3-least-squares-fitting-with-qr)
4. [Graphics transforms](#4-graphics-transforms)
5. [Memory model](#5-memory-model)
6. [Numerical accuracy and limitations](#6-numerical-accuracy-and-limitations)

---

## 1. Creating and manipulating matrices

### Basic matrix construction

```turmeric
(import linalg/mat :refer [mat-new mat-new-zeroed mat-identity mat-from-list
                            mat-free mat-print])

;; Create a 3x4 uninitialized matrix
(let [A (mat-new 3 4)]
  (mat-free A))

;; Create a 2x2 zero matrix
(let [B (mat-new-zeroed 2 2)]
  (mat-print B)  ;; prints: 0.0000 0.0000
              ;;         0.0000 0.0000
  (mat-free B))

;; Create a 3x3 identity matrix
(let [I (mat-identity 3)]
  (mat-print I)  ;; prints: 1.0000 0.0000 0.0000
              ;;         0.0000 1.0000 0.0000
              ;;         0.0000 0.0000 1.0000
  (mat-free I))

;; Create a matrix from a list (row-major)
(let [C (mat-from-list 2 3 (cons 1.0 (cons 2.0 (cons 3.0
                                    (cons 4.0 (cons 5.0 (cons 6.0 0)))))))]
  (mat-print C)  ;; prints: 1.0000 2.0000 3.0000
              ;;         4.0000 5.0000 6.0000
  (mat-free C))
```

### Matrix arithmetic

```turmeric
(import linalg/mat :refer [mat-add mat-sub mat-scale mat-mul mat-transpose
                            mat-trace mat-norm-fro mat-copy])

(let [A (mat-from-list 2 2 (cons 1.0 (cons 2.0 (cons 3.0 (cons 4.0 0)))))
      B (mat-from-list 2 2 (cons 5.0 (cons 6.0 (cons 7.0 (cons 8.0 0)))))]
  
  ;; Addition
  (let [C (mat-add A B)]
    (mat-print C)  ;; prints: 6.0000 8.0000
                ;;         10.0000 12.0000
    (mat-free C))
  
  ;; Scalar multiplication
  (let [C (mat-scale A 2.0)]
    (mat-print C)  ;; prints: 2.0000 4.0000
                ;;         6.0000 8.0000
    (mat-free C))
  
  ;; Matrix multiplication
  (let [D (mat-from-list 2 3 (cons 1.0 (cons 2.0 (cons 3.0
                                    (cons 4.0 (cons 5.0 (cons 6.0 0)))))))
        E (mat-from-list 3 2 (cons 7.0 (cons 8.0 (cons 9.0
                                    (cons 10.0 (cons 11.0 (cons 12.0 0)))))))
        F (mat-mul D E)]
    (mat-print F)  ;; prints: 58.0000 64.0000
                ;;         139.0000 154.0000
    (mat-free D)
    (mat-free E)
    (mat-free F))
  
  ;; Transpose
  (let [C (mat-transpose A)]
    (mat-print C)  ;; prints: 1.0000 3.0000
                ;;         2.0000 4.0000
    (mat-free C))
  
  ;; Trace (sum of diagonal)
  (println (mat-trace A))  ;; prints: 5.0
  
  ;; Frobenius norm
  (println (mat-norm-fro A))  ;; prints: 5.4772 (sqrt(1+4+9+16))
  
  (mat-free A)
  (mat-free B))
```

### Vector operations

```turmeric
(import linalg/vec :refer [vec-new vec-from-list vec-free vec-add vec-sub
                            vec-scale vec-dot vec-norm vec-normalize])

(let [a (vec-from-list (cons 1.0 (cons 2.0 (cons 3.0 0))))
      b (vec-from-list (cons 4.0 (cons 5.0 (cons 6.0 0)))]
  
  ;; Addition
  (let [c (vec-add a b)]
    (vec-print c)  ;; prints: (5.0000 7.0000 9.0000)
    (vec-free c))
  
  ;; Dot product
  (println (vec-dot a b))  ;; prints: 32.0 (1*4 + 2*5 + 3*6)
  
  ;; Norm
  (println (vec-norm a))  ;; prints: 3.7417 (sqrt(1+4+9))
  
  ;; Normalize
  (let [u (vec-normalize a)]
    (vec-print u)  ;; prints: (0.2673 0.5345 0.8018)
    (vec-free u))
  
  (vec-free a)
  (vec-free b))
```

---

## 2. Solving linear systems

### Cholesky decomposition (for SPD matrices)

```turmeric
(import linalg/decomp :refer [chol chol-free])
(import linalg/solve :refer [chol-solve])

;; Solve Ax = b where A is symmetric positive definite
(let [A (mat-from-list 3 3 (cons 4.0 (cons -1.0 (cons 2.0
                                    (cons -1.0 (cons 5.0 (cons 0.0
                                    (cons 2.0 (cons 0.0 (cons 3.0 0)))))))))]
      b (vec-from-list (cons 1.0 (cons 2.0 (cons 3.0 0))))
      
      ;; Factorize A = L L'
      f (chol A)
      
      ;; Solve using the factor
      x (chol-solve f b)]
  
  (vec-print x)  ;; prints solution
  
  (chol-free f)
  (mat-free A)
  (vec-free b)
  (vec-free x))
```

### LU decomposition (for general square matrices)

```turmeric
(import linalg/decomp :refer [lu lu-free])
(import linalg/solve :refer [lu-solve mat-solve])

;; Solve Ax = b where A is any square non-singular matrix
(let [A (mat-from-list 3 3 (cons 1.0 (cons 2.0 (cons 3.0
                                    (cons 4.0 (cons 5.0 (cons 6.0
                                    (cons 7.0 (cons 8.0 (cons 9.0 0)))))))))]
      b (vec-from-list (cons 1.0 (cons 0.0 (cons 0.0 0))))
      
      ;; Option 1: Factorize and solve separately
      f (lu A)
      x (lu-solve f b)
      
      ;; Option 2: Factorize and solve in one call (automatically chooses LU)
      x2 (mat-solve A b 0)]
  
  (vec-print x)
  (vec-print x2)
  
  (lu-free f)
  (mat-free A)
  (vec-free b)
  (vec-free x)
  (vec-free x2))
```

### Matrix inverse and determinant

```turmeric
(import linalg/solve :refer [mat-inv mat-det])

(let [A (mat-from-list 2 2 (cons 1.0 (cons 2.0 (cons 3.0 (cons 4.0 0)))))
      
      ;; Compute inverse
      Ainv (mat-inv A)
      
      ;; Compute determinant
      det (mat-det A)]
  
  (println "Inverse:")
  (mat-print Ainv)
  
  (println "Determinant:")
  (println det)  ;; prints: -2.0
  
  (mat-free A)
  (mat-free Ainv))
```

---

## 3. Least-squares fitting with QR

```turmeric
(import linalg/decomp :refer [qr qr-free])
(import linalg/solve :refer [qr-solve])

;; Solve min ||Ax - b|| for overdetermined system (m > n)
;; This is the least-squares solution
(let [A (mat-from-list 6 3 (cons 1.0 (cons 1.0
                                    (cons 1.0 (cons 1.0
                                    (cons 1.0 (cons 1.0
                                    (cons 2.0 (cons 2.0
                                    (cons 2.0 (cons 3.0
                                    (cons 3.0 (cons 3.0 0))))))))))))]
      b (vec-from-list (cons 1.0 (cons 2.0 (cons 3.0
                                   (cons 4.0 (cons 5.0 (cons 6.0 0))))))
      
      ;; Factorize A = QR
      f (qr A)
      
      ;; Solve for least-squares solution
      x (qr-solve f b)]
  
  (vec-print x)
  
  (qr-free f)
  (mat-free A)
  (vec-free b)
  (vec-free x))
```

---

## 4. Graphics transforms

### Basic transforms

```turmeric
(import linalg/small :refer [vec3 mat4-identity mat4-translate mat4-scale
                               mat4-rotate-x mat4-rotate-y mat4-rotate-z
                               mat4-mul mat4-print mat4-ptr])

(let [eye (vec3 0.0 0.0 5.0)
      center (vec3 0.0 0.0 0.0)
      up (vec3 0.0 1.0 0.0)
      
      ;; Create transform matrices
      T (mat4-translate eye)
      Rx (mat4-rotate-x 0.5)
      Ry (mat4-rotate-y 1.0)
      
      ;; Combine: model = Ry * Rx * T
      model (mat4-mul (mat4-mul Ry Rx) T)]
  
  (mat4-print model)
  
  (free eye)
  (free center)
  (free up)
  (free T)
  (free Rx)
  (free Ry)
  (free model))
```

### Projection matrices

```turmeric
(import linalg/small :refer [mat4-perspective mat4-ortho mat4-look-at])

(let [;; Perspective projection
      proj (mat4-perspective 1.0  ; fovy (radians)
                            16.0/9.0  ; aspect
                            0.1       ; near
                            100.0)    ; far
      
      ;; View matrix
      eye (vec3 0.0 0.0 5.0)
      center (vec3 0.0 0.0 0.0)
      up (vec3 0.0 1.0 0.0)
      view (mat4-look-at eye center up)
      
      ;; Orthographic projection
      ortho (mat4-ortho -10.0 10.0  ; left, right
                         -10.0 10.0  ; bottom, top
                         0.1 100.0)] ; near, far
  
  (mat4-print proj)
  (mat4-print view)
  (mat4-print ortho)
  
  (free eye)
  (free center)
  (free up)
  (free proj)
  (free view)
  (free ortho))
```

### Passing to OpenGL

```turmeric
(import linalg/small :refer [mat4-ptr vec3-ptr])

;; mat4-ptr returns a pointer suitable for glUniformMatrix4fv
;; The pointer is valid only for the duration of the expression
(let [m (mat4-identity)
      ptr (mat4-ptr m)]
  ;; glUniformMatrix4fv(location, 1, GL_FALSE, ptr)
  (free m))
```

---

## 5. Memory model

### Ownership of matrices and vectors

All `mat` and `vec` types are heap-allocated. You must call `mat-free` or
`vec-free` to release memory when you're done with them:

```turmeric
(let [m (mat-new 10 10)]
  ;; ... use m ...
  (mat-free m))  ; Must free!
```

### Ownership of factor structs

Decomposition factor structs (`chol-factor`, `lu-factor`, `qr-factor`) own
their component matrices. You must free the factor to release all memory:

```turmeric
(let [A (mat-from-list 3 3 ...)
      f (chol A)]
  ;; ... use f ...
  (chol-free f)  ; Frees both the factor and the internal L matrix
  (mat-free A))
```

**Important:** Do not free the component matrices directly. Always free
through the factor's free function.

### Stack lifetime of small types

The `vec2`, `vec3`, `vec4`, `mat4` types from `linalg/small` are heap-allocated
in Turmeric (via `malloc`), so they follow the same ownership rules as `mat`
and `vec`. However, their pointers (`mat4-ptr`, `vec3-ptr`) are only valid for
the duration of the calling expression:

```turmeric
(let [m (mat4-identity)]
  (let [ptr (mat4-ptr m)]
    ;; Use ptr immediately
    )
  ;; ptr is no longer valid here!
  (free m))
```

---

## 6. Numerical accuracy and limitations

### Precision

- `linalg/mat` and `linalg/vec`: 64-bit floats (`double`)
- `linalg/small`: 32-bit floats (`float`)

### Algorithm limitations

- **Cholesky**: Requires symmetric positive-definite matrices. The algorithm
  detects non-SPD matrices by checking for non-positive pivots.
- **LU**: Uses partial pivoting. Detects singular matrices by checking for
  zero pivots within tolerance (1e-12).
- **QR**: Uses Householder reflections. Handles rank-deficient matrices by
  skipping zero columns.

### Matrix sizes

The pure Turmeric implementations are suitable for small to medium matrices
(typically up to a few hundred elements). For very large matrices or
production numerical work, consider:

1. Using a specialized numerical library
2. Adding a BLAS/LAPACK backend (planned for v0.2)
3. Using iterative methods for sparse systems

### Tolerances

- Cholesky and LU check for zero pivots with tolerance 1e-12
- `vec-normalize` rejects zero vectors with norm < 1e-6
- `mat-approx-eq?` and `vec-approx-eq?` accept a user-specified tolerance

---

## See also

- [API Reference](../../spices/linalg/) - Detailed function documentation
- [tur-math](https://github.com/rjungemann/turmeric-spices/tree/main/spices/math) - Alternative vector/math library
- [tur-stats](https://github.com/rjungemann/turmeric-spices/tree/main/spices/stats) - Statistics library that uses tur-linalg
