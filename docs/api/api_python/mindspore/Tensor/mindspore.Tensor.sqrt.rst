mindspore.Tensor.sqrt
=====================

.. py:method:: mindspore.Tensor.sqrt()

    逐元素返回当前Tensor的平方。

    .. math::
        y_i = \\sqrt(x_i)

    参数：
        - **x** (Tensor) - 任意维度的输入Tensor。该值必须大于0。

    返回：
        Tensor，具有与当前Tensor相同的数据类型和shape。

    异常：
        - **TypeError** - 输出不是Tensor
