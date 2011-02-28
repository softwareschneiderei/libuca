
#include <stdlib.h>
#include <clser.h>
#include <fgrab_struct.h>
#include <fgrab_prototyp.h>

#include "uca.h"
#include "uca-grabber.h"

#define GET_FG(grabber) ((Fg_Struct *)(grabber->user))

uint32_t uca_me4_destroy(struct uca_grabber_t *grabber)
{
    Fg_FreeGrabber(GET_FG(grabber));
}

uint32_t uca_me4_set_property(struct uca_grabber_t *grabber, enum uca_property_ids property, void *data)
{
    return Fg_setParameter(GET_FG(grabber), property, data, PORT_A) == FG_OK ? UCA_NO_ERROR : UCA_ERR_PROP_GENERAL;
}

uint32_t uca_me4_get_property(struct uca_grabber_t *grabber, enum uca_property_ids property, void *data)
{
    return Fg_getParameter(GET_FG(grabber), property, data, PORT_A) == FG_OK ? UCA_NO_ERROR : UCA_ERR_PROP_GENERAL;
}

uint32_t uca_me4_init(struct uca_grabber_t **grabber)
{
    /* FIXME: find out if this board/grabber is running */
    Fg_Struct *fg = Fg_Init("libFullAreaGray8.so", 0);
    if (fg == NULL)
        return UCA_ERR_INIT_NOT_FOUND;

    struct uca_grabber_t *uca = (struct uca_grabber_t *) malloc(sizeof(struct uca_grabber_t));
    uca->user = fg;
    uca->destroy = &uca_me4_destroy;
    uca->set_property = &uca_me4_set_property;
    uca->get_property = &uca_me4_get_property;
    
    *grabber = uca;
    return UCA_NO_ERROR;
}
