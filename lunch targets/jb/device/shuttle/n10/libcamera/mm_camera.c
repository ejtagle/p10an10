
int v4l2_s_ctrl( int fd,  uint32_t id, int32_t value)
{
    int rc = 0;
    struct v4l2_control control;

    memset(&control, 0, sizeof(control));
    control.id = id;
    control.value = value;
    rc = ioctl (fd, VIDIOC_S_CTRL, &control);

    if(rc) {
        CDBG("%s: fd=%d, S_CTRL, id=0x%x, value = 0x%x, rc = %ld\n",
                 __func__, fd, id, (uint32_t)value, rc);
   }
    return rc;
}

int v4l2_g_ctrl( int fd, uint32_t id, int32_t *value)
{
    int rc = 0;
  struct v4l2_control control;

    memset(&control, 0, sizeof(control));
    control.id = id;
    control.value = (int32_t)value;
    rc = ioctl (fd, VIDIOC_G_CTRL, &control);
    if(rc) {
        CDBG("%s: fd=%d, G_CTRL, id=0x%x, rc = %d\n", __func__, fd, id, rc);
     }
    *value = control.value;
    return rc;
}


int v4l2_query_ctrl( int fd, uint32_t id, int32_t *min, int32_t* max)
{
    int rc = 0;
	struct v4l2_queryctrl query;

    memset(&query, 0, sizeof(query));
    query.id = id;
    rc = ioctl (fd, VIDIOC_QUERYCTRL, &query);
    if(rc) {
        CDBG("%s: fd=%d, QUERYCTRL, id=0x%x, rc = %d\n", __func__, fd, id, rc);
    }else {
		if (query.flags & V4L2_CTRL_FLAG_DISABLED) {
			rc = -EINVAL;
		}
	}
    *min = query.minimum;
	*max = query.maximum;

    return rc;
}

static int v4l2_set_antibanding (int fd, int antibanding) {
    int rc = 0;
    struct v4l2_control ctrl;

    ctrl.id = V4L2_CID_POWER_LINE_FREQUENCY;
    ctrl.value = antibanding;
    rc = ioctl(fd, VIDIOC_S_CTRL, &ctrl);
    return rc;
}

static int v4l2_set_auto_focus (int fd, int value)
{
    int rc = 0;
    struct v4l2_queryctrl queryctrl;

    memset (&queryctrl, 0, sizeof (queryctrl));
    queryctrl.id = V4L2_CID_FOCUS_AUTO;

    if(value != 0 && value != 1) {
        CDBG("%s:boolean required, invalid value = %d\n",__func__, value);
        return --EINVAL;
    }
    if (-1 == ioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
        CDBG ("V4L2_CID_FOCUS_AUTO is not supported\n");
    } else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
        CDBG ("%s:V4L2_CID_FOCUS_AUTO is not supported\n", __func__);
    } else {
        if(0 != (rc =  v4l2_s_ctrl(fd,
                V4L2_CID_FOCUS_AUTO, value))){
            CDBG("%s: error, id=0x%x, value=%d, rc = %d\n",
                     __func__, V4L2_CID_FOCUS_AUTO, value, rc);
            rc = -1;
        }
    }
    return rc;
}

static int32_t v4l2_set_whitebalance (int fd, int mode) {

    int rc = 0, value;
    uint32_t id;

    switch(mode) {
    case MM_CAMERA_WHITE_BALANCE_AUTO:
        id = V4L2_CID_AUTO_WHITE_BALANCE;
        value = 1; /* TRUE */
        break;
    case MM_CAMERA_WHITE_BALANCE_OFF:
        id = V4L2_CID_AUTO_WHITE_BALANCE;
        value = 0; /* FALSE */
        break;
    default:
        id = V4L2_CID_WHITE_BALANCE_TEMPERATURE;
        if(mode == WHITE_BALANCE_DAYLIGHT) value = 6500;
        else if(mode == WHITE_BALANCE_INCANDESCENT) value = 2800;
        else if(mode == WHITE_BALANCE_FLUORESCENT ) value = 4200;
        else if(mode == WHITE_BALANCE_CLOUDY) value = 7500;
        else
            value = 4200;
    }
    if(0 != (rc =  v4l2_s_ctrl(fd,
            id, value))){
        CDBG("%s: error, exp_metering_action_param=%d, rc = %d\n", __func__, value, rc);
        goto end;
    }
end:
    return rc;
}

static int32_t v4l2_set_toggle_afr (int fd) {
    int rc = 0;
    int value = 0;
    if(0 != (rc =  v4l2_g_ctrl(fd,
            V4L2_CID_EXPOSURE_AUTO, &value))){
        goto end;
    }
    /* V4L2_CID_EXPOSURE_AUTO needs to be AUTO or SHUTTER_PRIORITY */
    if (value != V4L2_EXPOSURE_AUTO && value != V4L2_EXPOSURE_SHUTTER_PRIORITY) {
    CDBG("%s: V4L2_CID_EXPOSURE_AUTO needs to be AUTO/SHUTTER_PRIORITY\n",
        __func__);
    return -1;
  }
    if(0 != (rc =  v4l2_g_ctrl(fd,
            V4L2_CID_EXPOSURE_AUTO_PRIORITY, &value))){
        goto end;
    }
    value = !value;
    if(0 != (rc =  v4l2_s_ctrl(fd,
            V4L2_CID_EXPOSURE_AUTO_PRIORITY, value))){
        goto end;
    }
end:
    return rc;
}


int32_t mm_camera_set_general_parm(mm_camera_obj_t * my_obj, mm_camera_parm_t *parm)
{
    int rc = -MM_CAMERA_E_NOT_SUPPORTED;

    switch(parm->parm_type)  {
    case MM_CAMERA_PARM_SHARPNESS:
        return v4l2_s_ctrl(fd, V4L2_CID_SHARPNESS,
                                                                            *((int *)(parm->p_value)));
    case MM_CAMERA_PARM_CONTRAST:
        return v4l2_s_ctrl(fd, V4L2_CID_CONTRAST,
                                                                            *((int *)(parm->p_value)));
    case MM_CAMERA_PARM_SATURATION:
        return v4l2_s_ctrl(fd, V4L2_CID_SATURATION,
                                                                            *((int *)(parm->p_value)));
    case MM_CAMERA_PARM_BRIGHTNESS:
        return v4l2_s_ctrl(fd, V4L2_CID_BRIGHTNESS,
                                                                            *((int *)(parm->p_value)));
    case MM_CAMERA_PARM_WHITE_BALANCE:
        return v4l2_set_whitebalance (my_obj, *((int *)(parm->p_value)));
    case MM_CAMERA_PARM_ZOOM:
        return v4l2_s_ctrl(fd, V4L2_CID_ZOOM_ABSOLUTE,
                                                                            *((int *)(parm->p_value)));
    case MM_CAMERA_PARM_ANTIBANDING:
        return v4l2_set_antibanding (my_obj, *((int *)(parm->p_value)));
    case MM_CAMERA_PARM_CONTINUOUS_AF:
        return v4l2_set_auto_focus(my_obj, *((int *)(parm->p_value)));
    case MM_CAMERA_PARM_EFFECT:
        return v4l2_set_specialEffect (my_obj, *((int *)(parm->p_value)));
 \   default:
        CDBG("%s: default: parm %d not supported\n", __func__, parm->parm_type);
        break;
    }
    return rc;
}
