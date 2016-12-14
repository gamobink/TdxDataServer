
(defun println (msg &optional (stream t))
  (format stream "~A~%" msg))

(defun print-list (list)
  (loop for item in list
	   do (println item)))

(defun path-join (head tail)
  (string-concat (string-right-trim '(#\/ #\\) head)
				 "\\"
				 (string-left-trim '(#\/ #\\) tail)))

(defun file-content (filespec &rest open-args)
  (with-open-stream (stream (apply #'open filespec open-args))
	(let* ((buffer (make-array (file-length stream)
							   :element-type (stream-element-type stream)
							   :fill-pointer t))
		   (position (read-sequence buffer stream)))
	  (setf (fill-pointer buffer) position)
	  buffer)))


; ���ݻص�������
(defconstant PER_MIN5 0)  ; 5��������
(defconstant PER_MIN15 1) ; 15��������
(defconstant PER_MIN30 2) ; 30��������
(defconstant PER_HOUR 3)  ; 1Сʱ����
(defconstant PER_DAY 4)   ; ��������
(defconstant PER_WEEK 5)  ; ��������
(defconstant PER_MONTH	6) ; ��������
(defconstant PER_MIN1 7)  ; 1��������
(defconstant PER_MINN 8)  ; ���������(10)
(defconstant PER_DAYN 9)  ; ����������(45)
(defconstant PER_SEASON 10) ; ��������
(defconstant PER_YEAR 11) ; ��������
(defconstant PER_SEC5 12) ; 5����
(defconstant PER_SECN 13) ; ������(15)
(defconstant PER_PRD_DIY0 14) ; DIY����
(defconstant PER_PRD_DIY10 24); DIY����

(defconstant REPORT_DAT2 102) ; ��������(�ڶ���)
(defconstant GBINFO_DAT 103)  ; �ɱ���Ϣ
(defconstant STKINFO_DAT 105) ; ��Ʊ�������

(defconstant TPPRICE_DAT 121) ; �ǵ�ͣ����

; market type
(defconstant SH 1)
(defconstant SZ 0)

(defun read-server-port ()
  (parse-integer (string-trim " \r\n"
							  (file-content
							   "C:\\zd_cczq\\DataServer\\server-port.txt"))))

(defclass time ()
  ((year :initarg :year)
   (month :initarg :month)
   (day :initarg :day)
   (hour :initarg :hour :initform 0)
   (minute :initarg :minute :initform 0)
   (second :initarg :second :initform 0)))

(defmethod print-object ((obj time) stream)
  (format stream "~A-~A-~A ~A:~A:~A"
		  (slot-value obj 'year)
		  (slot-value obj 'month)
		  (slot-value obj 'day)
		  (slot-value obj 'hour)
		  (slot-value obj 'minute)
		  (slot-value obj 'second)))

(defun make-time (&key year month day hour minute second)
  (multiple-value-bind (- - - nday nmonth nyear) (get-decoded-time)
	(make-instance 'time
				   :year (or year nyear)
				   :month (or month nmonth)
				   :day (or day nday)
				   :hour (or hour 0)
				   :minute (or minute 0)
				   :second (or second 0))))

(defun make-request (&key market code type from to)
  (unless (and market code type) (error "invalid arguments: market/code/type"))
  (with-output-to-string (s)
	(format s "Market:~A~%" market)
	(format s "Code:~A~%" code)
	(format s "Type:~A~%" type)
	(when from (format s "From:~A~%" from))
	(when to (format s "To:~A~%" to))))

(defun query (request)
  (WITH-OPEN-STREAM (socket
					 (SOCKET:SOCKET-CONNECT
					  (read-server-port)
					  "127.0.0.1"
					  :EXTERNAL-FORMAT :DOS))
    (format socket request)
    ;; not necessary if the server understands the "Content-length" header
    (SOCKET:SOCKET-STREAM-SHUTDOWN socket :output)
    ;; get the server response
    (LOOP :for line = (READ-LINE socket nil nil) :while line :collect line)))

(query (make-request
		:market SH
		:code "600256"
		:type PER_DAY
		:from (make-time :month 1 :day 1)
		:to (make-time)))
