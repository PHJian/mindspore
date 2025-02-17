# Copyright 2020-2022 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================
"""
Testing UniformAugment in DE
"""
import numpy as np
import pytest

import mindspore.dataset as ds
import mindspore.dataset.transforms
import mindspore.dataset.vision as vision
from mindspore import log as logger
from util import visualize_list, diff_mse, config_get_set_seed

DATA_DIR = "../data/dataset/testImageNetData/train/"


def test_uniform_augment_callable(num_ops=2):
    """
    Feature: UniformAugment
    Description: Test UniformAugment under execute mode
    Expectation: Output's shape is the same as expected output's shape
    """
    logger.info("test_uniform_augment_callable")
    img = np.fromfile("../data/dataset/apple.jpg", dtype=np.uint8)
    logger.info("Image.type: {}, Image.shape: {}".format(type(img), img.shape))

    decode_op = vision.Decode()
    img = decode_op(img)
    assert img.shape == (2268, 4032, 3)

    transforms_ua = [vision.RandomCrop(size=[200, 400], padding=[32, 32, 32, 32]),
                     vision.RandomCrop(size=[200, 400], padding=[32, 32, 32, 32])]
    uni_aug = vision.UniformAugment(transforms=transforms_ua, num_ops=num_ops)
    img = uni_aug(img)
    assert img.shape == (2268, 4032, 3) or img.shape == (200, 400, 3)


def test_uniform_augment_callable_pil(num_ops=2):
    """
    Feature: UniformAugment
    Description: Test UniformAugment under execute mode, with PIL input.
    Expectation: Output's shape is the same as expected output's shape
    """
    logger.info("test_uniform_augment_callable")
    img = np.fromfile("../data/dataset/apple.jpg", dtype=np.uint8)
    logger.info("Image.type: {}, Image.shape: {}".format(type(img), img.shape))

    decode_op = vision.Decode(to_pil=True)
    img = decode_op(img)
    assert img.size == (4032, 2268)

    transforms_ua = [vision.RandomCrop(size=[200, 400], padding=[32, 32, 32, 32]),
                     vision.RandomCrop(size=[200, 400], padding=[32, 32, 32, 32])]
    uni_aug = vision.UniformAugment(transforms=transforms_ua, num_ops=num_ops)
    img = uni_aug(img)
    assert img.size == (4032, 2268) or img.size == (400, 200)


def test_uniform_augment_callable_pil_pyfunc(num_ops=3):
    """
    Feature: UniformAugment
    Description: Test UniformAugment under execute mode, with PIL input. Include pyfunc in transforms list.
    Expectation: Output's shape is the same as expected output's shape
    """
    logger.info("test_uniform_augment_callable")
    img = np.fromfile("../data/dataset/apple.jpg", dtype=np.uint8)
    logger.info("Image.type: {}, Image.shape: {}".format(type(img), img.shape))

    decode_op = vision.Decode(to_pil=True)
    img = decode_op(img)
    assert img.size == (4032, 2268)

    transforms_ua = [vision.RandomCrop(size=[200, 400], padding=[32, 32, 32, 32]),
                     lambda x: x,
                     vision.RandomCrop(size=[200, 400], padding=[32, 32, 32, 32])]
    uni_aug = vision.UniformAugment(transforms=transforms_ua, num_ops=num_ops)
    img = uni_aug(img)
    assert img.size == (4032, 2268) or img.size == (400, 200)


def test_uniform_augment_callable_tuple(num_ops=2):
    """
    Feature: UniformAugment
    Description: Test UniformAugment under execute mode. Use tuple for transforms list argument.
    Expectation: Output's shape is the same as expected output's shape
    """
    logger.info("test_uniform_augment_callable")
    img = np.fromfile("../data/dataset/apple.jpg", dtype=np.uint8)
    logger.info("Image.type: {}, Image.shape: {}".format(type(img), img.shape))

    decode_op = vision.Decode()
    img = decode_op(img)
    assert img.shape == (2268, 4032, 3)

    transforms_ua = (vision.RandomCrop(size=[200, 400], padding=[32, 32, 32, 32]),
                     vision.RandomCrop(size=[200, 400], padding=[32, 32, 32, 32]))
    uni_aug = vision.UniformAugment(transforms=transforms_ua, num_ops=num_ops)
    img = uni_aug(img)
    assert img.shape == (2268, 4032, 3) or img.shape == (200, 400, 3)


def test_uniform_augment(plot=False, num_ops=2):
    """
    Feature: UniformAugment
    Description: Test UniformAugment using Python implementation
    Expectation: Output is the same as expected output
    """
    logger.info("Test UniformAugment")

    # Original Images
    data_set = ds.ImageFolderDataset(dataset_dir=DATA_DIR, shuffle=False)

    transforms_original = mindspore.dataset.transforms.Compose([vision.Decode(True),
                                                                vision.Resize((224, 224)),
                                                                vision.ToTensor()])

    ds_original = data_set.map(operations=transforms_original, input_columns="image")

    ds_original = ds_original.batch(512)

    for idx, (image, _) in enumerate(ds_original):
        if idx == 0:
            images_original = np.transpose(image.asnumpy(), (0, 2, 3, 1))
        else:
            images_original = np.append(images_original,
                                        np.transpose(image.asnumpy(), (0, 2, 3, 1)),
                                        axis=0)

    # UniformAugment Images
    data_set = ds.ImageFolderDataset(dataset_dir=DATA_DIR, shuffle=False)

    transform_list = [vision.RandomRotation(45),
                      vision.RandomColor(),
                      vision.RandomSharpness(),
                      vision.Invert(),
                      vision.AutoContrast(),
                      vision.Equalize()]

    transforms_ua = \
        mindspore.dataset.transforms.Compose([vision.Decode(True),
                                              vision.Resize((224, 224)),
                                              vision.UniformAugment(transforms=transform_list,
                                                                    num_ops=num_ops),
                                              vision.ToTensor()])

    ds_ua = data_set.map(operations=transforms_ua, input_columns="image")

    ds_ua = ds_ua.batch(512)

    for idx, (image, _) in enumerate(ds_ua):
        if idx == 0:
            images_ua = np.transpose(image.asnumpy(), (0, 2, 3, 1))
        else:
            images_ua = np.append(images_ua,
                                  np.transpose(image.asnumpy(), (0, 2, 3, 1)),
                                  axis=0)

    num_samples = images_original.shape[0]
    mse = np.zeros(num_samples)
    for i in range(num_samples):
        mse[i] = diff_mse(images_ua[i], images_original[i])
    logger.info("MSE= {}".format(str(np.mean(mse))))

    if plot:
        visualize_list(images_original, images_ua)


def test_uniform_augment_pyfunc(num_ops=2, my_seed=1):
    """
    Feature: UniformAugment
    Description: Test UniformAugment using Python implementation.   Include pyfunc in transforms list.
    Expectation: Output is the same as expected output
    """
    logger.info("Test UniformAugment with pyfunc")
    original_seed = config_get_set_seed(my_seed)
    logger.info("my_seed= {}".format(str(my_seed)))

    # Original Images
    data_set = ds.ImageFolderDataset(dataset_dir=DATA_DIR, shuffle=False)

    transforms_original = mindspore.dataset.transforms.Compose([vision.Decode(True)])

    ds_original = data_set.map(operations=transforms_original, input_columns="image")

    ds_original = ds_original.batch(512)

    for idx, (image, _) in enumerate(ds_original):
        if idx == 0:
            images_original = np.transpose(image.asnumpy(), (0, 2, 3, 1))
        else:
            images_original = np.append(images_original,
                                        np.transpose(image.asnumpy(), (0, 2, 3, 1)),
                                        axis=0)

    # UniformAugment Images
    data_set = ds.ImageFolderDataset(dataset_dir=DATA_DIR, shuffle=False)

    transform_list = [vision.Invert(),
                      lambda x: x,
                      vision.AutoContrast(),
                      vision.Equalize()
                      ]

    transforms_ua = \
        mindspore.dataset.transforms.Compose([vision.Decode(True),
                                              vision.UniformAugment(transforms=transform_list, num_ops=num_ops)])

    ds_ua = data_set.map(operations=transforms_ua, input_columns="image")

    ds_ua = ds_ua.batch(512)

    for idx, (image, _) in enumerate(ds_ua):
        if idx == 0:
            images_ua = np.transpose(image.asnumpy(), (0, 2, 3, 1))
        else:
            images_ua = np.append(images_ua,
                                  np.transpose(image.asnumpy(), (0, 2, 3, 1)),
                                  axis=0)

    num_samples = images_original.shape[0]
    mse = np.zeros(num_samples)
    for i in range(num_samples):
        mse[i] = diff_mse(images_ua[i], images_original[i])
    logger.info("MSE= {}".format(str(np.mean(mse))))

    # Restore configuration
    ds.config.set_seed(original_seed)


def test_cpp_uniform_augment(plot=False, num_ops=2):
    """
    Feature: UniformAugment
    Description: Test UniformAugment using Cpp implementation
    Expectation: Output is the same as expected output
    """
    logger.info("Test CPP UniformAugment")

    # Original Images
    data_set = ds.ImageFolderDataset(dataset_dir=DATA_DIR, shuffle=False)

    transforms_original = [vision.Decode(), vision.Resize(size=[224, 224]),
                           vision.ToTensor()]

    ds_original = data_set.map(operations=transforms_original, input_columns="image")

    ds_original = ds_original.batch(512)

    for idx, (image, _) in enumerate(ds_original):
        if idx == 0:
            images_original = np.transpose(image.asnumpy(), (0, 2, 3, 1))
        else:
            images_original = np.append(images_original,
                                        np.transpose(image.asnumpy(), (0, 2, 3, 1)),
                                        axis=0)

    # UniformAugment Images
    data_set = ds.ImageFolderDataset(dataset_dir=DATA_DIR, shuffle=False)
    transforms_ua = [vision.RandomCrop(size=[224, 224], padding=[32, 32, 32, 32]),
                     vision.RandomHorizontalFlip(),
                     vision.RandomVerticalFlip(),
                     vision.RandomColorAdjust(),
                     vision.RandomRotation(degrees=45)]

    uni_aug = vision.UniformAugment(transforms=transforms_ua, num_ops=num_ops)

    transforms_all = [vision.Decode(), vision.Resize(size=[224, 224]),
                      uni_aug,
                      vision.ToTensor()]

    ds_ua = data_set.map(operations=transforms_all, input_columns="image", num_parallel_workers=1)

    ds_ua = ds_ua.batch(512)

    for idx, (image, _) in enumerate(ds_ua):
        if idx == 0:
            images_ua = np.transpose(image.asnumpy(), (0, 2, 3, 1))
        else:
            images_ua = np.append(images_ua,
                                  np.transpose(image.asnumpy(), (0, 2, 3, 1)),
                                  axis=0)
    if plot:
        visualize_list(images_original, images_ua)

    num_samples = images_original.shape[0]
    mse = np.zeros(num_samples)
    for i in range(num_samples):
        mse[i] = diff_mse(images_ua[i], images_original[i])
    logger.info("MSE= {}".format(str(np.mean(mse))))


def test_cpp_uniform_augment_exception_large_numops(num_ops=6):
    """
    Feature: UniformAugment
    Description: Test UniformAugment using invalid large number of ops
    Expectation: Exception is raised as expected
    """
    logger.info("Test CPP UniformAugment invalid large num_ops exception")

    transforms_ua = [vision.RandomCrop(size=[224, 224], padding=[32, 32, 32, 32]),
                     vision.RandomHorizontalFlip(),
                     vision.RandomVerticalFlip(),
                     vision.RandomColorAdjust(),
                     vision.RandomRotation(degrees=45)]

    with pytest.raises(ValueError) as error_info:
        _ = vision.UniformAugment(transforms=transforms_ua, num_ops=num_ops)
    logger.info("Got an exception in DE: {}".format(str(error_info)))
    assert "num_ops is greater than transforms list size" in str(error_info)


def test_cpp_uniform_augment_exception_nonpositive_numops(num_ops=0):
    """
    Feature: UniformAugment
    Description: Test UniformAugment using invalid non-positive num_ops
    Expectation: Exception is raised as expected
    """
    logger.info("Test UniformAugment invalid non-positive num_ops exception")

    transforms_ua = [vision.RandomCrop(size=[224, 224], padding=[32, 32, 32, 32]),
                     vision.RandomHorizontalFlip(),
                     vision.RandomVerticalFlip(),
                     vision.RandomColorAdjust(),
                     vision.RandomRotation(degrees=45)]

    with pytest.raises(ValueError) as error_info:
        _ = vision.UniformAugment(transforms=transforms_ua, num_ops=num_ops)
    logger.info("Got an exception in DE: {}".format(str(error_info)))
    assert "Input num_ops must be greater than 0" in str(error_info)


def test_cpp_uniform_augment_exception_float_numops(num_ops=2.5):
    """
    Feature: UniformAugment
    Description: Test UniformAugment using invalid float num_ops
    Expectation: Exception is raised as expected
    """
    logger.info("Test UniformAugment invalid float num_ops exception")

    transforms_ua = [vision.RandomCrop(size=[224, 224], padding=[32, 32, 32, 32]),
                     vision.RandomHorizontalFlip(),
                     vision.RandomVerticalFlip(),
                     vision.RandomColorAdjust(),
                     vision.RandomRotation(degrees=45)]

    with pytest.raises(TypeError) as error_info:
        _ = vision.UniformAugment(transforms=transforms_ua, num_ops=num_ops)
    logger.info("Got an exception in DE: {}".format(str(error_info)))
    assert "Argument num_ops with value 2.5 is not of type [<class 'int'>]" in str(error_info)


def test_cpp_uniform_augment_random_crop_badinput(num_ops=1):
    """
    Feature: UniformAugment
    Description: Test UniformAugment with greater crop size
    Expectation: Exception is raised as expected
    """
    logger.info("Test UniformAugment with random_crop bad input")
    batch_size = 2
    cifar10_dir = "../data/dataset/testCifar10Data"
    ds1 = ds.Cifar10Dataset(cifar10_dir, shuffle=False)  # shape = [32,32,3]

    transforms_ua = [
        # Note: crop size [224, 224] > image size [32, 32]
        vision.RandomCrop(size=[224, 224]),
        vision.RandomHorizontalFlip()
    ]
    uni_aug = vision.UniformAugment(transforms=transforms_ua, num_ops=num_ops)
    ds1 = ds1.map(operations=uni_aug, input_columns="image")

    # apply DatasetOps
    ds1 = ds1.batch(batch_size, drop_remainder=True, num_parallel_workers=1)
    num_batches = 0
    with pytest.raises(RuntimeError) as error_info:
        for _ in ds1.create_dict_iterator(num_epochs=1, output_numpy=True):
            num_batches += 1
    assert "Shape is incorrect. map operation: [UniformAugment] failed." in str(error_info)


if __name__ == "__main__":
    test_uniform_augment_callable()
    test_uniform_augment_callable_pil()
    test_uniform_augment_callable_pil_pyfunc()
    test_uniform_augment_callable_tuple()
    test_uniform_augment(num_ops=6, plot=True)
    test_uniform_augment_pyfunc(num_ops=2, my_seed=1)
    test_cpp_uniform_augment(num_ops=1, plot=True)
    test_cpp_uniform_augment_exception_large_numops(num_ops=6)
    test_cpp_uniform_augment_exception_nonpositive_numops(num_ops=0)
    test_cpp_uniform_augment_exception_float_numops(num_ops=2.5)
    test_cpp_uniform_augment_random_crop_badinput(num_ops=1)
