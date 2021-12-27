#ifndef EDS3_CHECK_H
#define EDS3_CHECK_H

#ifdef __cplusplus
#define CHECK(OBJ) ({ auto _tmp = OBJ; if (_tmp == nullptr) return JNI_ERR; _tmp; })
#else
#define CHECK(OBJ) ({ void *_tmp = OBJ; if (_tmp == NULL) return JNI_ERR; _tmp; })
#endif

#endif //EDS3_CHECK_H
