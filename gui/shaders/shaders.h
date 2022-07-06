#ifndef shaders_h
#define shaders_h

/* Vertex shaders */
extern const char _binary_simpleproj_vert[];
extern unsigned long _binary_simpleproj_vert_size;
extern const char _binary_skewtproj_vert[];
extern const unsigned long _binary_skewtproj_vert_size;
extern const char _binary_bezierproj_vert[];
extern const unsigned long _binary_bezierproj_vert_size;
extern const char _binary_worldproj_vert[];
extern const unsigned long _binary_worldproj_vert_size;

/* Fragment shaders */
extern const char _binary_simplecolor_frag[];
extern const unsigned long _binary_simplecolor_frag_size;
extern const char _binary_feathercolor_frag[];
extern const unsigned long _binary_feathercolor_frag_size;

#endif
