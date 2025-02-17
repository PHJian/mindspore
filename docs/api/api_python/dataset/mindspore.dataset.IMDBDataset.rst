mindspore.dataset.IMDBDataset
=============================

.. py:class:: mindspore.dataset.IMDBDataset(dataset_dir, usage=None, num_samples=None, num_parallel_workers=None, shuffle=None, sampler=None, num_shards=None, shard_id=None, cache=None)

    读取和解析互联网电影数据库（IMDb）的源数据集。

    生成的数据集有两列 `[text, label]` ， `text` 列的数据类型是string。 `label` 列的数据类型是uint32。

    参数：
        - **dataset_dir** (str) - 包含数据集文件的根目录路径。
        - **usage** (str, 可选) - 指定数据集的子集，可取值为'train'，'test'或'all'。默认值：None，读取全部样本。
        - **num_samples** (int, 可选) - 指定从数据集中读取的样本数。
          对于Polarity数据集，'train'将读取360万个训练样本，'test'将读取40万个测试样本，'all'将读取所有400万个样本。
          对于Full数据集，'train'将读取300万个训练样本，'test'将读取65万个测试样本，'all'将读取所有365万个样本。默认值：None，读取所有样本。
        - **num_parallel_workers** (int, 可选) - 指定读取数据的工作线程数。默认值：None，使用mindspore.dataset.config中配置的线程数。
        - **shuffle** (Union[bool, Shuffle], 可选) - 每个epoch中数据混洗的模式，支持传入bool类型与枚举类型进行指定，默认值：mindspore.dataset.Shuffle.GLOBAL。
          如果 `shuffle` 为False，则不混洗，如果 `shuffle` 为True，等同于将 `shuffle` 设置为mindspore.dataset.Shuffle.GLOBAL。
          通过传入枚举变量设置数据混洗的模式：

          - **Shuffle.GLOBAL**：混洗文件和样本。
          - **Shuffle.FILES**：仅混洗文件。

        - **sampler** (Sampler, 可选) - 指定从数据集中选取样本的采样器，默认值：None，下表中会展示不同配置的预期行为。
        - **num_shards** (int, 可选) - 指定分布式训练时将数据集进行划分的分片数，默认值：None。指定此参数后， `num_samples` 表示每个分片的最大样本数。
        - **shard_id** (int, 可选) - 指定分布式训练时使用的分片ID号，默认值：None。只有当指定了 `num_shards` 时才能指定此参数。
        - **cache** (DatasetCache, 可选) - 单节点数据缓存服务，用于加快数据集处理，详情请阅读 `单节点数据缓存 <https://www.mindspore.cn/tutorials/experts/zh-CN/master/dataset/cache.html>`_ 。默认值：None，不使用缓存。

    异常：
        - **RuntimeError** - `dataset_dir` 参数所指向的文件目录不存在或缺少数据集文件。
        - **ValueError** - `num_parallel_workers` 参数超过系统最大线程数。
        - **RuntimeError** - 同时指定了 `sampler` 和 `shuffle` 参数。
        - **RuntimeError** - 同时指定了 `sampler` 和 `num_shards` 参数或同时指定了 `sampler` 和 `shard_id` 参数。
        - **RuntimeError** - 指定了 `num_shards` 参数，但是未指定 `shard_id` 参数。
        - **RuntimeError** - 指定了 `shard_id` 参数，但是未指定 `num_shards` 参数。
        - **ValueError** - `shard_id` 参数值错误（小于0或者大于等于 `num_shards` ）。

    .. note:: 此数据集可以指定参数 `sampler` ，但参数 `sampler` 和参数 `shuffle` 的行为是互斥的。下表展示了几种合法的输入参数组合及预期的行为。

    .. list-table:: 配置 `sampler` 和 `shuffle` 的不同组合得到的预期排序结果
       :widths: 25 25 50
       :header-rows: 1

       * - 参数 `sampler`
         - 参数 `shuffle`
         - 预期数据顺序
       * - None
         - None
         - 随机排列
       * - None
         - True
         - 随机排列
       * - None
         - False
         - 顺序排列
       * - `sampler` 实例
         - None
         - 由 `sampler` 行为定义的顺序
       * - `sampler` 实例
         - True
         - 不允许
       * - `sampler` 实例
         - False
         - 不允许

    **关于IMDB数据集：**

    IMDB数据集包含来自互联网电影数据库(IMDB)的50000条高度两极分化的评论。
    数据集分为25,000条用于训练的评论和25,000条用于测试的评论，训练集和测试集都包含50%的积极评论和50%的消极评论。
    训练标签和测试标签分别是0和1，其中0代表负样本，1代表正样本。
        
    可以将数据集文件解压缩到此目录结构中，并通过MindSpore的API读取。

    .. code-block::

        .
        └── imdb_dataset_directory
                ├── train
                │    ├── pos
                │    │    ├── 0_9.txt
                │    │    ├── 1_7.txt
                │    │    ├── ...
                │    ├── neg
                │    │    ├── 0_3.txt
                │    │    ├── 1_1.txt
                │    │    ├── ...
                ├── test
                │    ├── pos
                │    │    ├── 0_10.txt
                │    │    ├── 1_10.txt
                │    │    ├── ...
                │    ├── neg
                │    │    ├── 0_2.txt
                │    │    ├── 1_3.txt
                │    │    ├── ...

    **引用：**

    .. code-block::
        
        @InProceedings{maas-EtAl:2011:ACL-HLT2011,
            author    = {Maas, Andrew L.  and  Daly, Raymond E.  and  Pham, Peter T.  and  Huang, Dan
                        and  Ng, Andrew Y.  and  Potts, Christopher},
            title     = {Learning Word Vectors for Sentiment Analysis},
            booktitle = {Proceedings of the 49th Annual Meeting of the Association for Computational Linguistics:
                        Human Language Technologies},
            month     = {June},
            year      = {2011},
            address   = {Portland, Oregon, USA},
            publisher = {Association for Computational Linguistics},
            pages     = {142--150},
            url       = {http://www.aclweb.org/anthology/P11-1015}
        }

    .. include:: mindspore.dataset.Dataset.add_sampler.rst

    .. include:: mindspore.dataset.Dataset.rst

    .. include:: mindspore.dataset.Dataset.b.rst

    .. include:: mindspore.dataset.Dataset.c.rst

    .. include:: mindspore.dataset.Dataset.d.rst

    .. include:: mindspore.dataset.Dataset.e.rst

    .. include:: mindspore.dataset.Dataset.f.rst

    .. include:: mindspore.dataset.Dataset.save.rst

    .. include:: mindspore.dataset.Dataset.g.rst

    .. include:: mindspore.dataset.Dataset.use_sampler.rst

    .. include:: mindspore.dataset.Dataset.zip.rst
