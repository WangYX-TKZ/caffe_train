#!/bin/sh
if ! test -f ../prototxt/model/face_train_v5.prototxt ;then
	echo "error: ../../prototxt/model/face_train_v5.prototxt does not exit."
	echo "please generate your own model prototxt primarily."
        exit 1
fi
if ! test -f ../prototxt/model/face_test_v5.prototxt ;then
	echo "error: ../../prototxt/model/face_test_v5.prototxt does not exit."
	echo "please generate your own model prototxt primarily."
        exit 1
fi
../../../../build/tools/caffe train --solver=../solver/solver_train_v5.prototxt -gpu 0 \
--snapshot=../snapshot/face_v5_iter_8580.solverstate
