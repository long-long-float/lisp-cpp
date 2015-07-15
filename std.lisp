; Suzuka Lisp Standard Module

(print "loaded std module")

(defmacro defun (name args body) (setq name (lambda args body)))
