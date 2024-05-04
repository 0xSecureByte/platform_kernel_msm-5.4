#include <kunit/test.h>
#include <kunit/fff.h>

#include <linux/module.h>
#include "../touchpanel_common.h"
#include "synaptics_common.h"

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(struct proc_dir_entry *, proc_create_data, const char *,
		umode_t, struct proc_dir_entry *, const struct  proc_ops *, void *);

/*synaptics_create_proc*/
static void synaptics_create_proc_case_0(struct kunit *test)
{
	struct touchpanel_data ts_fake;
	struct synaptics_proc_operations syna_ops_fake;
	int retval = 0;

	proc_create_data_fake.return_val = NULL;

    retval = synaptics_create_proc(&ts_fake, &syna_ops_fake);
    KUNIT_EXPECT_EQ(test, 0u, proc_create_data_fake.call_count);
    KUNIT_EXPECT_GT(test, 0, retval);
}

/*synaptics_remove_proc*/
FAKE_VOID_FUNC(remove_proc_entry, const char *, struct proc_dir_entry *);

static void synaptics_remove_proc_case_0(struct kunit *test)
{
	struct touchpanel_data ts_fake;
	struct synaptics_proc_operations syna_ops_fake;
	int retval = 0;

	proc_create_data_fake.return_val = NULL;

    retval = synaptics_remove_proc(&ts_fake, &syna_ops_fake);
    KUNIT_EXPECT_EQ(test, 0u, remove_proc_entry_fake.call_count);
    KUNIT_EXPECT_GT(test, 0, retval);
}

static struct kunit_case touch_test_cases[] = {

	KUNIT_CASE(synaptics_create_proc_case_0),

	KUNIT_CASE(synaptics_remove_proc_case_0),

	{}
};

static struct kunit_suite touch_test_suite = {
        .name = "touch_test",
        .test_cases = touch_test_cases,
};

kunit_test_suite(touch_test_suite);

MODULE_LICENSE("GPL v2");



